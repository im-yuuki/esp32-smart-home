#include "sht31.h"

#include <string.h>

// v6: driver headers no longer include FreeRTOS headers -- explicit includes.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "sht31";

#define SHT31_ADDR_PRIMARY  0x44
#define SHT31_ADDR_ALT      0x45
#define SHT31_SCL_HZ        100000
#define SHT31_I2C_TIMEOUT_MS 100
#define SHT31_MEAS_DELAY_MS 16  // single-shot high-repeatability conversion (~15 ms)

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
    uint8_t sda, scl;
    uint16_t addr;
} sht31_ctx_t;

static sht31_ctx_t s_ctx;

// CRC-8: poly 0x31, init 0xFF (SHT3x datasheet), over each 2-byte word.
static uint8_t sht31_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t add_device(sht31_ctx_t *ctx, uint16_t addr)
{
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = SHT31_SCL_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(ctx->bus, &dev_cfg, &ctx->dev);
    if (err == ESP_OK) {
        ctx->addr = addr;
    }
    return err;
}

static esp_err_t sht31_init(sensor_driver_t *drv)
{
    sht31_ctx_t *ctx = (sht31_ctx_t *)drv->ctx;

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,  // auto-select
        .sda_io_num = (gpio_num_t)ctx->sda,   // fields are gpio_num_t, not int
        .scl_io_num = (gpio_num_t)ctx->scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &ctx->bus);
    if (err != ESP_OK) {
        return err;
    }
    return add_device(ctx, SHT31_ADDR_PRIMARY);
}

static esp_err_t sht31_probe(sensor_driver_t *drv)
{
    sht31_ctx_t *ctx = (sht31_ctx_t *)drv->ctx;

    esp_err_t err = i2c_master_probe(ctx->bus, SHT31_ADDR_PRIMARY, SHT31_I2C_TIMEOUT_MS);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "SHT31 found at 0x44");
        return ESP_OK;
    }
    if (err == ESP_ERR_TIMEOUT) {
        // Distinguish: bus stuck / missing pull-ups vs "no sensor on bus".
        ESP_LOGW(TAG, "I2C bus timeout (SDA=%u SCL=%u): bus stuck or missing pull-ups",
                 (unsigned)ctx->sda, (unsigned)ctx->scl);
        return err;
    }

    // ESP_ERR_NOT_FOUND at 0x44 -> try the alternate address.
    err = i2c_master_probe(ctx->bus, SHT31_ADDR_ALT, SHT31_I2C_TIMEOUT_MS);
    if (err == ESP_OK) {
        esp_err_t aerr = i2c_master_bus_rm_device(ctx->dev);
        if (aerr != ESP_OK) {
            return aerr;
        }
        aerr = add_device(ctx, SHT31_ADDR_ALT);
        if (aerr != ESP_OK) {
            return aerr;
        }
        ESP_LOGI(TAG, "SHT31 found at alternate address 0x45");
        return ESP_OK;
    }
    return err;  // ESP_ERR_NOT_FOUND: no SHT31 on the bus
}

static esp_err_t sht31_read(sensor_driver_t *drv, float *temp_c, float *rh)
{
    sht31_ctx_t *ctx = (sht31_ctx_t *)drv->ctx;

    // Single-shot, high repeatability, no clock stretching: {0x24, 0x00}.
    // Deliberately NOT i2c_master_transmit_receive: its repeated-START leaves
    // no room for the ~15 ms conversion.
    const uint8_t cmd[2] = { 0x24, 0x00 };
    esp_err_t err = i2c_master_transmit(ctx->dev, cmd, sizeof cmd, SHT31_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        // v6: NACK => ESP_ERR_INVALID_RESPONSE (sensor vanished), not INVALID_STATE.
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(SHT31_MEAS_DELAY_MS));  // exact at CONFIG_FREERTOS_HZ=1000

    uint8_t buf[6];
    err = i2c_master_receive(ctx->dev, buf, sizeof buf, SHT31_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }

    if (sht31_crc8(buf, 2) != buf[2] || sht31_crc8(buf + 3, 2) != buf[5]) {
        return ESP_ERR_INVALID_CRC;
    }

    const uint16_t raw_t = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    const uint16_t raw_h = (uint16_t)(((uint16_t)buf[3] << 8) | buf[4]);
    *temp_c = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    *rh = 100.0f * (float)raw_h / 65535.0f;
    return ESP_OK;
}

static void sht31_deinit(sensor_driver_t *drv)
{
    sht31_ctx_t *ctx = (sht31_ctx_t *)drv->ctx;
    if (ctx->dev != NULL) {
        i2c_master_bus_rm_device(ctx->dev);
        ctx->dev = NULL;
    }
    if (ctx->bus != NULL) {
        i2c_del_master_bus(ctx->bus);
        ctx->bus = NULL;
    }
}

sensor_driver_t *sht31_get_driver(uint8_t sda_gpio, uint8_t scl_gpio)
{
    static sensor_driver_t drv = {
        .model = "SHT31",
        .kind = "temperature_humidity",
        .init = sht31_init,
        .probe = sht31_probe,
        .read = sht31_read,
        .deinit = sht31_deinit,
        .ctx = &s_ctx,
    };
    s_ctx.sda = sda_gpio;
    s_ctx.scl = scl_gpio;
    return &drv;
}
