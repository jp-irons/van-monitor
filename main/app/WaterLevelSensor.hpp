#pragma once

#include "DisplayContext.hpp"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include <cstdint>

namespace app {

/**
 * WaterLevelSensor — owns the ADC1 channel 9 (GPIO10) interface for the
 * 4-20 mA water-level sensor wired via a 150 Ω shunt (0.6–3.0 V).
 *
 * Lifecycle:
 *   call init() once from ApplicationContext::start() after NVS is ready.
 *   call poll() every loop() tick; it populates a display::WaterData struct
 *   that ApplicationContext forwards to DisplayContext and AppState.
 *
 * ADC strategy:
 *   - ADC1 only (ADC2 conflicts with Wi-Fi).
 *   - 12 dB attenuation → full-scale ≈ 3.3 V, covering the 3.0 V maximum.
 *   - 16 samples per poll; the middle 8 are averaged after sorting (drops the
 *     4 lowest and 4 highest to reject switching noise from the MPPT/inverter).
 *   - Hardware non-linearity is corrected via the ESP32-S3 curve-fitting
 *     calibration scheme; falls back to a linear mapping if unavailable.
 *
 * Calibration:
 *   Two-point voltage calibration (calVEmpty, calVFull) is read from NVS
 *   namespace "water_cal" — the same keys written by CalibrateHandler and
 *   CalibrateScreen — and refreshed every ~5 s (CAL_REFRESH_TICKS × 50 ms).
 *
 * Formula (after applying NVS calibration):
 *   level_m = (volts - calVEmpty) / (calVFull - calVEmpty) * 2.0
 */
class WaterLevelSensor {
public:
    static constexpr const char* TAG = "WaterLevelSensor";

    WaterLevelSensor();
    ~WaterLevelSensor();

    /** Open ADC unit, configure channel, create hw-calibration scheme.
     *  Returns true on success.  Safe to call before Wi-Fi is started. */
    bool init();

    /** Sample the ADC, apply calibration, and update @p out.
     *  @p tick is the caller's loop counter (monotonically increasing). */
    void poll(display::WaterData& out, uint32_t tick);

private:
    // ── ADC configuration ─────────────────────────────────────────────────
    static constexpr adc_unit_t    UNIT        = ADC_UNIT_1;
    static constexpr adc_channel_t CHANNEL     = ADC_CHANNEL_9; // GPIO10
    static constexpr adc_atten_t   ATTENUATION = ADC_ATTEN_DB_12;
    static constexpr int           SAMPLE_COUNT = 16;
    static constexpr int           DROP_EACH    = 4; // drop 4 low + 4 high

    // ── NVS calibration ───────────────────────────────────────────────────
    static constexpr const char* NVS_NS           = "water_cal";
    static constexpr uint32_t    CAL_REFRESH_TICKS = 100; // ≈ 5 s at 50 ms/tick

    // ── State ─────────────────────────────────────────────────────────────
    adc_oneshot_unit_handle_t adcUnit_  {};
    adc_cali_handle_t         adcCali_  {};
    bool                      caliOk_   {false};
    bool                      ready_    {false};

    float    calVEmpty_   {0.60f};
    float    calVFull_    {3.00f};
    uint32_t tankLitres_  {200};
    uint32_t lastCalTick_ {0};

    // IIR low-pass filter state.  Negative sentinel means "not yet seeded":
    // the first valid reading is used directly rather than blending from 0 V.
    // α = 0.99 at a 50 ms tick gives τ ≈ 5 s.
    static constexpr float IIR_ALPHA    = 0.99f;
    float                  smoothedVolts_ {-1.0f};

    // ── Helpers ───────────────────────────────────────────────────────────

    /** Read and refresh the NVS 2-point calibration into member fields. */
    void loadCalibrationFromNvs();

    /** Take SAMPLE_COUNT raw ADC readings, sort, average the middle band.
     *  Returns the averaged raw count, or -1 on hardware error. */
    int sampleAdc();

    /** Convert a raw ADC count to volts using the hw-calibration handle,
     *  or a linear approximation if curve-fitting was unavailable. */
    float adcToVoltage(int raw) const;
};

} // namespace app
