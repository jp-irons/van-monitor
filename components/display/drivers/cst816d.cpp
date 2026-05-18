#include "cst816d.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "cst816d";

// I2C register / address constants
static constexpr uint8_t  CST816D_ADDR     = 0x15;
static constexpr uint8_t  CST816D_REG_DATA = 0x01;   // first byte of 5-byte touch record
static constexpr uint32_t CST816D_I2C_HZ   = 400000; // 400 kHz fast mode
static constexpr int      I2C_TIMEOUT_MS   = 10;

namespace display {

// Module-level device handle — there is exactly one CST816D per board.
static i2c_master_dev_handle_t s_dev = nullptr;

esp_err_t cst816d_init(i2c_master_bus_handle_t bus,
                        gpio_num_t rst_gpio,
                        gpio_num_t int_gpio)
{
    ESP_LOGI(TAG, "Initialising CST816D (RST=GPIO%d, INT=GPIO%d)", rst_gpio, int_gpio);

    // ── Hard reset ────────────────────────────────────────────────────────
    gpio_config_t rst_cfg = {
        .pin_bit_mask = (1ULL << rst_gpio),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&rst_cfg);
    gpio_set_level(rst_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(rst_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(50));  // datasheet: 50 ms after reset before first I2C access

    // ── INT pin — input only, polling mode ───────────────────────────────
    if (int_gpio >= 0) {
        gpio_config_t int_cfg = {
            .pin_bit_mask = (1ULL << int_gpio),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&int_cfg);
    }

    // ── Register on I2C bus ───────────────────────────────────────────────
    // Value-initialise the whole struct first to avoid -Wmissing-field-initializers
    // on scl_wait_us and the flags union, both of which we leave at zero.
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = CST816D_ADDR;
    dev_cfg.scl_speed_hz    = CST816D_I2C_HZ;
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "CST816D ready");
    return ESP_OK;
}

TouchPoint cst816d_read()
{
    if (!s_dev) {
        return {0, 0, false};
    }

    // Write register address, then read 5 bytes in one transaction
    uint8_t reg = CST816D_REG_DATA;
    uint8_t buf[5] = {};

    esp_err_t err = i2c_master_transmit_receive(
        s_dev,
        &reg, sizeof(reg),
        buf,  sizeof(buf),
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );

    if (err != ESP_OK) {
        // Don't log here — this is called every LVGL tick (~5 ms)
        return {0, 0, false};
    }

    uint8_t count = buf[0] & 0x0F;  // lower nibble = touch point count
    if (count == 0) {
        return {0, 0, false};
    }

    uint16_t x = (static_cast<uint16_t>(buf[1] & 0x0F) << 8) | buf[2];
    uint16_t y = (static_cast<uint16_t>(buf[3] & 0x0F) << 8) | buf[4];

    return {x, y, true};
}

} // namespace display
