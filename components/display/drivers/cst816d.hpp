#pragma once

#include "driver/gpio.h"       // IWYU pragma: keep — gpio_num_t used in cst816d_init signature
#include "driver/i2c_master.h" // IWYU pragma: keep — i2c_master_bus_handle_t used in cst816d_init signature
#include "esp_err.h"
#include <cstdint>

/**
 * CST816D — single-touch capacitive touch controller driver.
 *
 * This is a lightweight, self-contained I2C reader.  It does NOT use the
 * esp_lcd_touch abstraction so it has no dependency on internal ESP-IDF
 * struct layouts that vary between IDF versions.
 *
 * Usage:
 *   1. Call cst816d_init() once during startup.
 *   2. Poll cst816d_read() from the LVGL input-device callback.
 *
 * I2C address : 0x15
 * Protocol    : Read 5 bytes from register 0x01
 *               byte[0] = touch count (0 or 1)
 *               byte[1] = X[11:8]  (high nibble)
 *               byte[2] = X[7:0]   (low byte)
 *               byte[3] = Y[11:8]  (high nibble)
 *               byte[4] = Y[7:0]   (low byte)
 */

namespace display {

struct TouchPoint {
    uint16_t x;
    uint16_t y;
    bool     pressed;
};

/**
 * Initialise the CST816D.
 *
 * Performs a hard reset via @p rst_gpio, then registers the device on the
 * supplied I2C master bus.  The internal device handle is stored in a module-
 * level static so cst816d_read() can be called without arguments.
 *
 * @param bus       Existing I2C master bus handle (already initialised).
 * @param rst_gpio  RST pin number (GPIO 16 on SpotPear board).
 * @param int_gpio  INT pin number (GPIO 17) — configured as input, not used
 *                  for interrupts in this driver (polling mode).
 */
esp_err_t cst816d_init(i2c_master_bus_handle_t bus,
                        gpio_num_t rst_gpio,
                        gpio_num_t int_gpio);

/**
 * Read the current touch state.
 *
 * Safe to call from any task.  Returns {0, 0, false} if no touch is active
 * or if the I2C read fails.
 */
TouchPoint cst816d_read();

} // namespace display
