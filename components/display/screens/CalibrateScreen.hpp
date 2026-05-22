#pragma once

#include "lvgl.h"

namespace display {

class DisplayContext;
struct WaterData;

/**
 * CalibrateScreen — page 2: tank calibration controls.
 *
 * Layout (240 × 320):
 *   [0  – 27]  Header bar: "CALIBRATE" (Montserrat 10, left-aligned)
 *   [34 – 69]  Tank capacity row: "Tank capacity" label + − value + button
 *   [78 – 137] Mark Empty button (blue): title + "press when tank is empty" subtitle
 *   [146– 205] Mark Full button (green): title + "press when tank is full" subtitle
 *   [214– 285] Raw readout panel — 3 rows (Raw level / Computed level / Cal points)
 *   [284– 319] Nav button
 *
 * Calibration points (empty voltage, full voltage) are persisted to NVS by
 * this screen.  The NVS key names match what WaterSensor will read:
 *   "cal_v_empty"  (float, default 0.60 V)
 *   "cal_v_full"   (float, default 3.00 V)
 *   "tank_litres"  (uint32, tank capacity in litres)
 *
 * The raw voltage observed at button-press time is taken from the last
 * WaterData pushed via updateRaw().
 */
class CalibrateScreen {
public:
    CalibrateScreen();

    void create(DisplayContext* ctx);
    void show();

    /** Refresh the raw-data readout panel. */
    void updateRaw(const WaterData& data);

private:
    // Button callbacks (static — user_data = this)
    static void onMarkEmpty(lv_event_t* e);
    static void onMarkFull(lv_event_t* e);
    static void onCapMinus(lv_event_t* e);
    static void onCapPlus(lv_event_t* e);

    void saveCalibration();
    void refreshCapLabel();

    lv_obj_t* screen_     {nullptr};

    // Capacity row
    lv_obj_t* capLabel_   {nullptr};  // "200 L"
    uint32_t  capacityL_  {200};      // current value, step 10 L

    // Raw readout labels
    lv_obj_t* rawAdc_      {nullptr};
    lv_obj_t* rawVolts_    {nullptr};
    lv_obj_t* computedM_   {nullptr};
    lv_obj_t* calPoints_   {nullptr};

    // Calibration state — updated when Mark buttons are pressed
    float calVEmpty_ {0.60f};
    float calVFull_  {3.00f};
    float lastVolts_ {0.0f};   // most recent reading from updateRaw()

    // Display-side IIR for Raw level label — slower than sensor IIR so the
    // label stays readable.  Seeded on first call (negative sentinel = unseeded).
    float dispVolts_ {-1.0f};
    static constexpr float DISP_ALPHA = 0.95f;
};

} // namespace display
