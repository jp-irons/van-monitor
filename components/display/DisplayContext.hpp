#pragma once

#include "screens/DashboardScreen.hpp"
#include "screens/SystemScreen.hpp"
#include "screens/CalibrateScreen.hpp"

#include "esp_lcd_panel_io.h"
#include "driver/i2c_master.h"
#include "lvgl.h"

#include <cstdint>
#include <functional>

namespace display {

// ── Data types passed in from ApplicationContext ─────────────────────────────

struct WaterData {
    float pct;     // 0.0 – 100.0
    float litres;
    float rawVolts;       // raw shunt voltage, for calibration screen
    uint16_t rawAdc;      // raw ADC count
    float computedM;      // computed depth in metres
};

struct BatteryData {
    float soc;            // 0.0 – 100.0  (from Venus OS)
    float voltage;        // V
    float current;        // A (positive = charging)
    float solarW;         // W
    float loadW;          // W
};

struct SystemData {
    const char* ssid;
    int         rssi;        // dBm
    const char* ipAddr;
    const char* hostname;
    bool        mqttOk;
    const char* venusPortalId;
    bool        venusOk;
    uint32_t    uptimeS;
    const char* firmwareVersion;
};

// ── DisplayContext ────────────────────────────────────────────────────────────

/**
 * DisplayContext — owns the full display stack for the van monitor.
 *
 * Responsibilities:
 *   - Initialise the ST7789T3 LCD via esp_lcd (SPI2_HOST).
 *   - Initialise the CST816D touch controller via I2C.
 *   - Initialise esp_lvgl_port and register a custom lv_indev for touch.
 *   - Create and own the three LVGL screens (Dashboard, System, Calibrate).
 *   - Expose data-push methods called by ApplicationContext::loop().
 *   - Cycle pages on nav-button press.
 *
 * Threading:
 *   All lv_* calls are guarded with lvgl_port_lock / lvgl_port_unlock so they
 *   are safe to call from the main-loop task (not the LVGL task).
 */
class DisplayContext {
public:
    static constexpr const char* TAG = "DisplayContext";

    DisplayContext();
    ~DisplayContext();

    /** Initialise hardware and LVGL; show the dashboard.  Call once from start(). */
    void start();

    /** Per-tick work — currently a no-op; LVGL runs its own task via esp_lvgl_port. */
    void loop();

    /**
     * Register a callback invoked on every touch-press event.
     * Called from the LVGL task — keep it short and non-blocking.
     * Typically wired to ActivityManager::poke() by ApplicationContext.
     */
    void setActivityCallback(std::function<void()> cb);

    /** Set backlight to full brightness (100 %). */
    void brighten();

    /** Set backlight to dim level (~15 %). */
    void dim();

    /** Push latest sensor readings to the dashboard screen. */
    void updateWaterLevel(const WaterData& data);
    void updateBattery(const BatteryData& data);

    /** Push connection / system state to the system-info screen. */
    void updateSystem(const SystemData& data);

    /** Advance to the next page (1 → 2 → 3 → 1).  Called by the nav-button callback. */
    void nextPage();

private:
    // ── Hardware init ─────────────────────────────────────────────────────
    void initSpi();
    void initLcd();
    void initBacklight();
    void initI2c();
    void initTouch();
    void initLvgl();

    // ── LVGL indev callback (static, calls instance via user_data) ────────
    static void touchReadCb(lv_indev_t* indev, lv_indev_data_t* data);

    // ── LCD / touch handles ───────────────────────────────────────────────
    esp_lcd_panel_io_handle_t ioHandle_    {nullptr};
    esp_lcd_panel_handle_t    panelHandle_ {nullptr};
    i2c_master_bus_handle_t   i2cBus_      {nullptr};
    lv_display_t*             lvglDisp_    {nullptr};
    lv_indev_t*               lvglTouch_   {nullptr};

    // ── Touch-activity callback (set by ApplicationContext) ───────────────
    std::function<void()> onTouchActivity_;

    // ── Screens ───────────────────────────────────────────────────────────
    DashboardScreen dashboard_;
    SystemScreen    sysInfo_;
    CalibrateScreen calibrate_;

    int currentPage_ {0};
    static constexpr int PAGE_COUNT = 3;
};

} // namespace display
