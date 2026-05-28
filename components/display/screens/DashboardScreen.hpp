#pragma once

#include "lvgl.h" // IWYU pragma: keep — lv_obj_t* used in member declarations

namespace display {

// Forward declaration — avoids circular include with DisplayContext.hpp
class DisplayContext;
struct WaterData;
struct BatteryData;
struct LevelData;

/**
 * DashboardScreen — page 1 of the van monitor UI.
 *
 * Layout (240 × 320):
 *   [0  – 27]  Header bar:  "VAN MONITOR" + Wi-Fi / MQTT status dots
 *   [28 – 73]  Water section: title, pct + litres row, bar
 *              Alarm state (≤10 % / ≥90 %): bar + text turn amber (no banner)
 *   [80 – 191] Level widget: 96×96 crosshair + dot + degree labels
 *              Dot colour: green ≤1°, amber 1–3°, red >3°
 *   [206– 283] Battery: "BATTERY" title, SOC row, bar, solar/load/net stats
 *              Alarm state (≤10 %): bar + SOC red.  (≥90 %): green label.
 *   [284– 319] Nav button: "tap to cycle pages >"
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

    /** Update the level crosshair widget. */
    void updateLevel(const LevelData& data);

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

    // ── Water section (compact bar) ───────────────────────────────────────
    lv_obj_t* waterAlarmRow_ {nullptr};  // full-width band; amber bg on alarm
    lv_obj_t* waterPct_      {nullptr};  // e.g. "63%"
    lv_obj_t* waterLitres_   {nullptr};  // e.g. "126 L"
    lv_obj_t* waterBar_      {nullptr};

    // ── Level widget ──────────────────────────────────────────────────────
    lv_obj_t* levelContainer_ {nullptr};
    lv_obj_t* levelDot_       {nullptr};
    lv_obj_t* levelLabelX_    {nullptr};  // left-axis degree readout
    lv_obj_t* levelLabelY_    {nullptr};  // right-axis degree readout

    // Previous formatted strings — used to suppress redundant label updates
    char prevLabelX_[10] = {};
    char prevLabelY_[10] = {};

    // ── Battery ───────────────────────────────────────────────────────────
    lv_obj_t* battAlarmRow_ {nullptr};  // full-width band; red bg on critical alarm
    lv_obj_t* battTitle_  {nullptr};  // "BATTERY" section label
    lv_obj_t* battSoc_    {nullptr};  // "84%"
    lv_obj_t* battVolts_  {nullptr};  // "27.3 V · 14.2 A"
    lv_obj_t* battBar_    {nullptr};
    lv_obj_t* battSolar_  {nullptr};  // "Solar 182W  Load 43W  Net +139W"
};

} // namespace display
