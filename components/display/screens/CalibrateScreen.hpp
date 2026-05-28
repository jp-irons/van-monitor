#pragma once

#include "lvgl.h"

#include <functional>

namespace display {

class DisplayContext;
struct WaterData;
struct LevelData;

/**
 * CalibrateScreen — page 2: sensor calibration controls.
 *
 * Layout (240 × 320):
 *   [0  –  27]  Header bar: "CALIBRATE"
 *   [36 –  71]  Tank capacity row: label + − / + buttons
 *   [80]        "WATER LEVEL - hold 1.5s" section label
 *   [94 – 197]  Combined water panel:
 *                 [94–131]  Empty sub-row  — cal voltage left, [Empty] right
 *                 [132–169] Full  sub-row  — cal voltage left, [Full]  right
 *                 [170–197] Voltage readout
 *   [206]       "TILT - hold 1.5s" section label
 *   [220– 259]  Zero row: live tilt angles left, [Zero] button right
 *   [284– 319]  Nav button  (24 px gap above)
 *
 * All three calibration actions (Empty, Full, Flat) require a 1.5-second hold
 * to prevent accidental triggering.
 *
 * NVS key names (namespace "water_cal"):
 *   "cal_v_empty_b"  (uint32, float bit-cast)
 *   "cal_v_full_b"   (uint32, float bit-cast)
 *   "tank_litres"    (uint32)
 *
 * Tilt calibration is delegated to ImuSensor via a callback set by
 * DisplayContext::setTiltFlatCallback().
 */
class CalibrateScreen {
public:
    static constexpr const char* TAG = "CalibrateScreen";

    CalibrateScreen();

    void create(DisplayContext* ctx);
    void show();

    /** Refresh the raw voltage / computed level readout. */
    void updateRaw(const WaterData& data);

    /** Refresh the live tilt readout in the raw panel. */
    void updateLevel(const LevelData& data);

    /**
     * Register the callback invoked when the user successfully long-presses
     * the Flat button.  Typically wired to ImuSensor::zero() by
     * ApplicationContext.
     */
    void setFlatCallback(std::function<void()> cb);

private:
    // ── Long-press context — one per calibration button ───────────────────
    struct BtnCtx {
        CalibrateScreen* self {nullptr};
        int              id   {0};  // 0=Empty, 1=Full, 2=Flat
    };

    static void onBtnPressed(lv_event_t* e);
    static void onBtnReleased(lv_event_t* e);   // handles RELEASED + PRESS_LOST

    void handleLongPress(int id);
    void updateCalDisplay();

    // ── Capacity row helpers ──────────────────────────────────────────────
    static void onCapMinus(lv_event_t* e);
    static void onCapPlus(lv_event_t* e);
    void saveCalibration();
    void refreshCapLabel();

    // ── Widgets ───────────────────────────────────────────────────────────
    lv_obj_t* screen_     {nullptr};

    // Capacity row
    lv_obj_t* capLabel_   {nullptr};
    uint32_t  capacityL_  {200};

    // Calibration buttons (stored so callbacks can adjust appearance)
    lv_obj_t* btnEmpty_   {nullptr};
    lv_obj_t* btnFull_    {nullptr};
    lv_obj_t* btnFlat_    {nullptr};

    // Inline calibration-point value labels (live in each cal row)
    lv_obj_t* emptyValLbl_ {nullptr};
    lv_obj_t* fullValLbl_  {nullptr};
    lv_obj_t* tiltLabel_   {nullptr};   // lives in the Flat row

    // Raw readout label (voltage only)
    lv_obj_t* rawVolts_    {nullptr};

    // ── Calibration state ─────────────────────────────────────────────────
    float calVEmpty_ {0.60f};
    float calVFull_  {3.00f};
    float lastVolts_ {0.0f};

    // Display-side IIR for voltage label
    float dispVolts_ {-1.0f};
    static constexpr float DISP_ALPHA = 0.95f;

    // ── Long-press state ──────────────────────────────────────────────────
    static constexpr uint32_t LONG_PRESS_MS = 1500;

    BtnCtx   btnCtx_[3]  = {};   // populated in create()
    uint32_t pressStart_ {0};
    int      pressedId_  {-1};

    // ── Flat callback ─────────────────────────────────────────────────────
    std::function<void()> onFlatCb_;
};

} // namespace display
