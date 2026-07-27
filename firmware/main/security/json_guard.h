// Nesting-depth pre-scan for UNTRUSTED JSON, shared by every parse site that
// takes bytes from outside the node: the captive portal (unauthenticated HTTP
// bodies) and the MQTT command path (broker-supplied payloads).
//
// cJSON's parser recurses once per nesting level (~64 bytes of stack each), so
// a few hundred bytes of "[[[[..." overrun a task stack -- the httpd task has
// 8 KB, the esp-mqtt task 6 KB against a 2 KB receive buffer.
// CONFIG_CJSON_NESTING_LIMIT (pinned to 16 in sdkconfig.defaults) is the
// primary bound, but sdkconfig.defaults only seeds a freshly generated
// sdkconfig -- making a single Kconfig value the only thing between a remote
// peer and a stack overflow is exactly the single point of failure this guard
// exists to remove. Run it before every cJSON parse of foreign data.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JSON_MAX_DEPTH 8  // deepest structure any of our schemas needs is 2

/**
 * @brief  True when the first @p len bytes of @p s nest no deeper than
 *         JSON_MAX_DEPTH.
 *
 * Length-taking because MQTT payloads are NOT NUL-terminated. Scans the full
 * length rather than stopping at an embedded NUL, because cJSON_ParseWithLength
 * does not stop there either (a NUL is <= 0x20 and skipped as whitespace).
 *
 * String-literal aware and backslash-escape aware, so brackets inside string
 * values -- including \" and \\ -- do not count. Counts '[' and '{' minus ']'
 * and '}'; the running depth may go negative on malformed input, which cJSON
 * rejects on its own.
 */
bool json_depth_ok_n(const char *s, size_t len);

#ifdef __cplusplus
}
#endif
