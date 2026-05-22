#include "cst816d.hpp"

#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "cst816d";

// I2C register / address constants
static constexpr uint8_t  CST816D_ADDR     = 0x15;
static constexpr uint8_t  CST816D_REG_DATA = 0x00;   // read full 7-byte packet from GestureID
static constexpr uint32_t CST816D_I2C_HZ   = 100000; // 100 kHz standard mode
static constexpr int      I2C_TIMEOUT_MS   = 20;      // ms — passed directly, NOT as ticks

namespace display {

// Module-level device handle — there is exactly one CST816D per board.
static i2c_master_dev_handle_t s_dev      = nullptr;
static gpio_num_t              s_int_gpio = GPIO_NUM_NC;

// Set by the INT falling-edge ISR; consumed (and cleared) by cst816d_read().
static volatile bool s_touch_pending = false;

static void IRAM_ATTR touch_isr_handler(void*)
{
    s_touch_pending = true;
}

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

    // ── INT pin — falling-edge interrupt, flag consumed by cst816d_read() ─
    if (int_gpio >= 0) {
        s_int_gpio = int_gpio;

        gpio_config_t int_cfg = {
            .pin_bit_mask = (1ULL << int_gpio),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_NEGEDGE,  // CST816 pulses INT low on touch
        };
        gpio_config(&int_cfg);

        // gpio_install_isr_service may already have been called elsewhere; that
        // returns ESP_ERR_INVALID_STATE, which is harmless — just skip it.
        esp_err_t isr_err = gpio_install_isr_service(0);
        if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "gpio_install_isr_service: %s", esp_err_to_name(isr_err));
        }
        gpio_isr_handler_add(int_gpio, touch_isr_handler, nullptr);
        ESP_LOGI(TAG, "Touch interrupt registered (GPIO%d, falling edge)", int_gpio);
    }

    // ── Register on I2C bus ───────────────────────────────────────────────
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
    if (!s_dev || !s_touch_pending) {
        return {0, 0, false};
    }
    s_touch_pending = false;

    // ── I2C read — 7 bytes from register 0x00 ────────────────────────────
    // Layout: [GestureID, FingerNum, XH, XL, YH, YL, misc]
    uint8_t reg = CST816D_REG_DATA;
    uint8_t buf[7] = {};

    esp_err_t err = i2c_master_transmit_receive(
        s_dev,
        &reg, sizeof(reg),
        buf,  sizeof(buf),
        I2C_TIMEOUT_MS          // milliseconds, not ticks
    );

    if (err != ESP_OK) {
        ESP_LOGI(TAG, "I2C read failed: %s", esp_err_to_name(err));
        return {0, 0, false};
    }

    // buf[1] = touch status: 0x05 = active, 0x00 = idle (stale data)
    // buf[4] = X (8-bit, 0–240)
    // buf[5:6] = Y big-endian 16-bit (0–319)
    if (buf[1] == 0x00) {
        return {0, 0, false};
    }

    uint16_t x = buf[4];
    uint16_t y = (static_cast<uint16_t>(buf[5]) << 8) | buf[6];

    return {x, y, true};
}

} // namespace display
