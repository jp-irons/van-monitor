#pragma once

#include "lvgl.h"

namespace display {

// Forward declaration — avoids circular include with DisplayContext.hpp
class DisplayContext;
struct WaterData;
struct BatteryData;

/**
 * DashboardScreen — page 1 of the van monitor UI.
 *
 * Layout (240 × 320):
 *   [0  – 27]  Header bar:  "VAN MONITOR" + Wi-Fi / MQTT status dots
 *   [28 – 47]  Warning banner (hidden when values are in normal range)
 *   [28 – 189] Water arc gauge: percentage + litres label
 *   [190]      Horizontal divider
 *   [191– 283] Battery: SOC row, bar, stats row (solar / load / net)
 *   [284– 319] Nav button: "tap to cycle pages >"
 *
 * Alarm thresholds are defined in Theme.hpp (ALARM_LOW_PCT / ALARM_HIGH_PCT).
 * At low water: arc + text turn amber, banner shows "WATER LOW".
 * At high water: arc + text turn amber, banner shows "TANK NEARLY FULL".
 * At low battery: bar + SOC text turn red, battery label shows "Battery critical".
 * At high battery: positive green indicator, label shows "Nearly charged".
 */
class DashboardScreen {
public:
    DashboardScreen() = default;

    /** Build all LVGL widgets.  Must be called inside lvgl_port_lock. */
    void create(DisplayContext* ctx);

    /** Load this screen as the active LVGL display. */
    void show();

    /** Update water-level widgets and apply alarm state if needed. */
    void updateWater(const WaterData& data);

    /** Update battery widgets and apply alarm state if needed. */
    void updateBattery(const BatteryData& data);

    /** Set the Wi-Fi and MQTT status dot colours. */
    void setWifiOk(bool ok);
    void setMqttOk(bool ok);

private:
    void applyWaterAlarm(float pct);
    void applyBatteryAlarm(float soc);
    lv_obj_t* makeNavButton(lv_obj_t* parent, DisplayContext* ctx);

    // ── Screen root ───────────────────────────────────────────────────────
    lv_obj_t* screen_  {nullptr};

    // ── Header ────────────────────────────────────────────────────────────
    lv_obj_t* dotWifi_ {nullptr};
    lv_obj_t* dotMqtt_ {nullptr};

    // ── Warning banner ────────────────────────────────────────────────────
    lv_obj_t* banner_     {nullptr};
    lv_obj_t* bannerLabel_{nullptr};

    // ── Water gauge ───────────────────────────────────────────────────────
    lv_obj_t* arc_       {nullptr};
    lv_obj_t* arcPct_    {nullptr};  // e.g. "63%"
    lv_obj_t* arcLitres_ {nullptr};  // e.g. "126 L"

    // ── Battery ───────────────────────────────────────────────────────────
    lv_obj_t* battIcon_   {nullptr};
    lv_obj_t* battSoc_    {nullptr};  // "84%"
    lv_obj_t* battVolts_  {nullptr};  // "27.3 V · 14.2 A"
    lv_obj_t* battBar_    {nullptr};
    lv_obj_t* battStatus_ {nullptr};  // "Solar 182 W  Load 43 W  Net +139 W"
};

} // namespace display
