/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
// Vendored from ESP-IDF v6.0.2 examples/protocols/http_server/captive_portal/
// components/dns_server (license header above kept verbatim). Local patches:
// explicit FreeRTOS includes, quieter per-packet logging, and a clean-stop
// path (SO_RCVTIMEO poll + exit semaphore) so stop_dns_server() no longer
// force-deletes the task while it blocks in recvfrom -- upstream leaked the
// UDP fd on every stop, and this portal starts/stops repeatedly.
//
// Further local patches:
// - Bind to the SoftAP address instead of INADDR_ANY. Upstream listens on
//   every interface, which in APSTA mode publishes the DNS hijack to the whole
//   home LAN through the STA address. The bind address is derived from the
//   config's existing per-entry netif/IP info, so dns_server.h is unchanged:
//   for the captive-portal use case the address we answer WITH is also the
//   address we must listen ON.
// - Error paths: no double close() of a fd lwIP may already have reissued, and
//   a failed bind() ends the task instead of spinning on recvfrom timeouts
//   forever with a DNS server that answers nothing.
// - start_dns_server() waits for the task to report its bind result, so a
//   socket()/bind() failure is a NULL return the caller can act on instead of a
//   live handle in front of a DNS server that answers nothing.
// - Unsigned DNS label lengths and a checked esp_netif_get_ip_info(): a 0xFF
//   length byte or a vanished netif came from the open AP unauthenticated.
// - stop_dns_server() only frees the handle when the task actually exited.

#include <sys/param.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "dns_server.h"

#define DNS_PORT (53)
#define DNS_MAX_LEN (256)

#define OPCODE_MASK (0x7800)
#define QR_FLAG (1 << 7)
#define QD_TYPE_A (0x0001)
#define ANS_TTL_SEC (300)

static const char *TAG = "dns_hijack";

// DNS Header Packet
typedef struct __attribute__((__packed__))
{
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

// DNS Question Packet
typedef struct {
    uint16_t type;
    uint16_t class;
} dns_question_t;

// DNS Answer Packet
typedef struct __attribute__((__packed__))
{
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
} dns_answer_t;

// DNS server handle
struct dns_server_handle {
    volatile bool started;      // patched: cleared by stop_dns_server (and by the task when it gives up), polled by the task
    SemaphoreHandle_t exited;   // patched: given by the task right before self-delete
    SemaphoreHandle_t bound;    // patched: given by the task once the bind result is known
    TaskHandle_t task;
    uint32_t bind_addr;         // patched: SoftAP IPv4 (network order) to bind to, never INADDR_ANY
    int num_of_entries;
    dns_entry_pair_t entry[];
};

/*
    Patched: pick the address to bind the DNS socket to out of the configured
    rules -- the netif's current IP for an if_key entry, otherwise the entry's
    static IP. Returns IPADDR_ANY when nothing is resolvable, which the caller
    treats as a start failure rather than silently listening everywhere.
*/
static uint32_t resolve_bind_addr(const dns_server_config_t *config)
{
    for (int i = 0; i < config->num_of_entries; ++i) {
        if (config->item[i].if_key != NULL) {
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey(config->item[i].if_key);
            esp_netif_ip_info_t ip_info;
            if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK
                && ip_info.ip.addr != IPADDR_ANY) {
                return ip_info.ip.addr;
            }
        } else if (config->item[i].ip.addr != IPADDR_ANY) {
            return config->item[i].ip.addr;
        }
    }
    return IPADDR_ANY;
}

/*
    Parse the name from the packet from the DNS name format to a regular .-seperated name
    returns the pointer to the next part of the packet
*/
static char *parse_dns_name(char *raw_name, char *parsed_name, size_t parsed_name_max_len)
{

    char *label = raw_name;
    char *name_itr = parsed_name;
    size_t name_len = 0;  // patched: size_t, to match parsed_name_max_len and the unsigned label length

    do {
        // Patched: the DNS label length is an UNSIGNED byte. Reading it through
        // a plain char only happens to work because ESP toolchains define
        // __CHAR_UNSIGNED__; under -fsigned-char a 0xFF length byte would sign-
        // extend to -1 and turn the memcpy below into memcpy(..., (size_t)-1),
        // i.e. an unauthenticated remote crash from the open SoftAP. Now that
        // this file is project-owned, pin the type instead of relying on it.
        unsigned int sub_name_len = (unsigned char)*label;
        // (len + 1) since we are adding  a '.'
        name_len += (sub_name_len + 1);
        if (name_len > parsed_name_max_len) {
            return NULL;
        }

        // Copy the sub name that follows the the label
        memcpy(name_itr, label + 1, sub_name_len);
        name_itr[sub_name_len] = '.';
        name_itr += (sub_name_len + 1);
        label += sub_name_len + 1;
    } while (*label != 0);

    // Terminate the final string, replacing the last '.'
    parsed_name[name_len - 1] = '\0';
    // Return pointer to first char after the name
    return label + 1;
}

// Parses the DNS request and prepares a DNS response with the IP of the softAP
static int parse_dns_request(char *req, size_t req_len, char *dns_reply, size_t dns_reply_max_len, dns_server_handle_t h)
{
    if (req_len > dns_reply_max_len) {
        return -1;
    }

    // Prepare the reply
    memset(dns_reply, 0, dns_reply_max_len);
    memcpy(dns_reply, req, req_len);

    // Endianess of NW packet different from chip
    dns_header_t *header = (dns_header_t *)dns_reply;
    ESP_LOGD(TAG, "DNS query with header id: 0x%X, flags: 0x%X, qd_count: %d",
             ntohs(header->id), ntohs(header->flags), ntohs(header->qd_count));

    // Not a standard query
    if ((header->flags & OPCODE_MASK) != 0) {
        return 0;
    }

    // Set question response flag
    header->flags |= QR_FLAG;

    uint16_t qd_count = ntohs(header->qd_count);
    header->an_count = htons(qd_count);

    int reply_len = qd_count * sizeof(dns_answer_t) + req_len;
    if (reply_len > dns_reply_max_len) {
        return -1;
    }

    // Pointer to current answer and question
    char *cur_ans_ptr = dns_reply + req_len;
    char *cur_qd_ptr = dns_reply + sizeof(dns_header_t);
    char name[128];

    // Respond to all questions based on configured rules
    for (int qd_i = 0; qd_i < qd_count; qd_i++) {
        char *name_end_ptr = parse_dns_name(cur_qd_ptr, name, sizeof(name));
        if (name_end_ptr == NULL) {
            ESP_LOGE(TAG, "Failed to parse DNS question: %s", cur_qd_ptr);
            return -1;
        }

        dns_question_t *question = (dns_question_t *)(name_end_ptr);
        uint16_t qd_type = ntohs(question->type);
        uint16_t qd_class = ntohs(question->class);

        ESP_LOGD(TAG, "Received type: %d | Class: %d | Question for: %s", qd_type, qd_class, name);

        if (qd_type == QD_TYPE_A) {
            esp_ip4_addr_t ip = { .addr = IPADDR_ANY };
            // Check the configured rules to decide whether to answer this question or not
            for (int i = 0; i < h->num_of_entries; ++i) {
                // check if the name either corresponds to the entry, or if we should answer to all queries ("*")
                if (strcmp(h->entry[i].name, "*") == 0 || strcmp(h->entry[i].name, name) == 0) {
                    if (h->entry[i].if_key) {
                        // Patched: upstream ignored the return value, so a NULL
                        // handle (netif gone) left ip_info uninitialized and
                        // leaked 4 bytes of this task's stack into the answer.
                        // On failure leave ip.addr == IPADDR_ANY, which the
                        // check below turns into "no rule applies".
                        esp_netif_t *netif = esp_netif_get_handle_from_ifkey(h->entry[i].if_key);
                        esp_netif_ip_info_t ip_info;
                        if (netif == NULL || esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
                            ESP_LOGW(TAG, "no IP for netif '%s' -- not answering", h->entry[i].if_key);
                        } else {
                            ip.addr = ip_info.ip.addr;
                        }
                        break;
                    } else if (h->entry[i].ip.addr != IPADDR_ANY) {  // patched: was h->entry->
                        ip.addr = h->entry[i].ip.addr;
                        break;
                    }
                }
            }
            if (ip.addr == IPADDR_ANY) {    // no rule applies, continue with another question
                continue;
            }
            dns_answer_t *answer = (dns_answer_t *)cur_ans_ptr;

            answer->ptr_offset = htons(0xC000 | (cur_qd_ptr - dns_reply));
            answer->type = htons(qd_type);
            answer->class = htons(qd_class);
            answer->ttl = htonl(ANS_TTL_SEC);

            ESP_LOGD(TAG, "Answer with PTR offset: 0x%" PRIX16 " and IP 0x%" PRIX32, ntohs(answer->ptr_offset), ip.addr);

            answer->addr_len = htons(sizeof(ip.addr));
            answer->ip_addr = ip.addr;
        }
    }
    return reply_len;
}

/*
    Sets up a socket and listen for DNS queries,
    replies to all type A queries with the IP of the softAP
*/
void dns_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;
    dns_server_handle_t handle = pvParameters;

    while (handle->started) {

        struct sockaddr_in dest_addr;
        // Patched: the SoftAP address, not INADDR_ANY -- see the header note.
        dest_addr.sin_addr.s_addr = handle->bind_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(DNS_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            // Patched: on every give-up path clear 'started' and release the
            // start handshake. 'started' is what the rest of the module reads
            // as "the hijack is running", and leaving it true meant
            // start_dns_server() handed the portal a live handle in front of a
            // DNS server that had already quit -- a silently non-hijacking
            // captive portal.
            handle->started = false;
            xSemaphoreGive(handle->bound);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            // Patched: upstream only logged and carried on, leaving the task
            // spinning on recvfrom timeouts behind a socket bound to nothing.
            ESP_LOGE(TAG, "Socket unable to bind to %s: errno %d -- stopping", addr_str, errno);
            close(sock);
            handle->started = false;  // see above
            xSemaphoreGive(handle->bound);
            break;
        }
        ESP_LOGI(TAG, "Socket bound to %s, port %d", addr_str, DNS_PORT);
        // Patched: unblock start_dns_server(). A binary semaphore that is
        // already available just refuses the give, so re-binding after a
        // recvfrom error (outer loop) is harmless -- nobody is waiting by then.
        xSemaphoreGive(handle->bound);

        // Patched: 500 ms receive timeout so the loop can observe
        // handle->started and exit cleanly on stop_dns_server().
        struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

        while (handle->started) {
            ESP_LOGD(TAG, "Waiting for data");
            struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue;  // patched: timeout poll -- loop re-checks handle->started
                }
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                close(sock);
                // Patched: mark the fd dead so the cleanup block below does not
                // close it a second time -- by then lwIP may have handed the
                // same descriptor to an httpd connection.
                sock = -1;
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                // Null-terminate whatever we received and treat like a string...
                rx_buffer[len] = 0;

                char reply[DNS_MAX_LEN];
                int reply_len = parse_dns_request(rx_buffer, len, reply, DNS_MAX_LEN, handle);

                ESP_LOGD(TAG, "Received %d bytes from %s | DNS reply with len: %d", len, addr_str, reply_len);
                if (reply_len <= 0) {
                    ESP_LOGE(TAG, "Failed to prepare a DNS reply");
                } else {
                    int err = sendto(sock, reply, reply_len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                    if (err < 0) {
                        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                        break;
                    }
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket");
            shutdown(sock, 0);
            close(sock);
        }
    }
    // Patched: socket is closed above; tell stop_dns_server() we are done.
    xSemaphoreGive(handle->exited);
    vTaskDelete(NULL);
}

dns_server_handle_t start_dns_server(dns_server_config_t *config)
{
    dns_server_handle_t handle = calloc(1, sizeof(struct dns_server_handle) + config->num_of_entries * sizeof(dns_entry_pair_t));
    ESP_RETURN_ON_FALSE(handle, NULL, TAG, "Failed to allocate dns server handle");

    handle->started = true;
    handle->num_of_entries = config->num_of_entries;
    memcpy(handle->entry, config->item, config->num_of_entries * sizeof(dns_entry_pair_t));

    // Patched: resolve the listen address before the task starts, so a missing
    // netif is a hard start failure instead of an INADDR_ANY fallback.
    handle->bind_addr = resolve_bind_addr(config);
    if (handle->bind_addr == IPADDR_ANY) {
        ESP_LOGE(TAG, "No resolvable interface/IP in the DNS config -- refusing to bind INADDR_ANY");
        free(handle);
        return NULL;
    }

    // Patched: exit semaphore lets stop_dns_server() wait for the task to
    // close its socket and self-delete instead of force-killing it.
    handle->exited = xSemaphoreCreateBinary();
    if (handle->exited == NULL) {
        free(handle);
        return NULL;
    }
    // Patched: bind handshake -- see below.
    handle->bound = xSemaphoreCreateBinary();
    if (handle->bound == NULL) {
        vSemaphoreDelete(handle->exited);
        free(handle);
        return NULL;
    }
    if (xTaskCreate(dns_server_task, "dns_server", 4096, handle, 5, &handle->task) != pdPASS) {
        vSemaphoreDelete(handle->bound);
        vSemaphoreDelete(handle->exited);
        free(handle);
        return NULL;
    }

    // Patched: wait for the task to report whether it got the socket. Without
    // this the caller cannot distinguish "hijacking" from "the task already
    // gave up on bind()", because the task starts asynchronously and at a lower
    // priority than the caller. A timeout is treated as success -- the same
    // optimistic assumption as upstream, only now it is the exception.
    if (xSemaphoreTake(handle->bound, pdMS_TO_TICKS(2000)) == pdTRUE && !handle->started) {
        ESP_LOGE(TAG, "DNS task failed to bind -- start aborted");
        // Same rule as stop_dns_server(): free ONLY once the task has actually
        // reported that it is gone. Until it gives handle->exited it still owns
        // both semaphores and the handle itself, so freeing on a timeout would
        // leave it giving a deleted semaphore inside freed memory. Leak ~40
        // bytes plus the task on purpose instead; the caller gets NULL either
        // way and the portal continues without the hijack.
        if (xSemaphoreTake(handle->exited, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ESP_LOGE(TAG, "DNS task did not exit within 2 s -- leaking the handle on purpose");
            return NULL;
        }
        vSemaphoreDelete(handle->bound);
        vSemaphoreDelete(handle->exited);
        free(handle);
        return NULL;
    }
    return handle;
}

void stop_dns_server(dns_server_handle_t handle)
{
    if (handle) {
        // Patched: signal the task and wait for it to exit -- it closes its
        // own socket. Upstream's vTaskDelete(handle->task) killed the task
        // while blocked in recvfrom and leaked the UDP fd.
        handle->started = false;
        if (xSemaphoreTake(handle->exited, pdMS_TO_TICKS(2000)) != pdTRUE) {
            // Patched: the task is still running and still owns handle->exited
            // and handle itself. Freeing them here would hand it a deleted
            // semaphore and freed memory to touch on its way out, so the
            // handle is leaked deliberately: ~40 bytes plus one task/socket
            // versus a near-certain crash. The portal keeps working; the next
            // start_dns_server() allocates a fresh handle.
            ESP_LOGE(TAG, "DNS task did not exit within 2 s -- leaking the handle on purpose");
            return;
        }
        vSemaphoreDelete(handle->bound);
        vSemaphoreDelete(handle->exited);
        free(handle);
    }
}
