#pragma once

#include "DisplayContext.hpp"
#include "driver/i2c_master.h"

namespace app {

/**
 * ImuSensor — polls the QMI8658A accelerometer and computes tilt angles
 * for the van-level display widget.
 *
 * Usage:
 *   Call init() once after DisplayContext::start() (I2C bus must be live).
 *   Call poll() each loop tick; it writes smoothed tilt angles in degrees
 *   into a LevelData struct.
 *
 *   Call zero() (via the Flat long-press on the calibration screen) when
 *   the van — or a test surface — is known to be level.  This captures the
 *   current tilt reading as a per-orientation offset and persists it to NVS.
 *
 * Orientation detection:
 *   Each poll the axis carrying the largest gravity component is identified
 *   as the dominant axis.  The other two become the tilt axes.  This
 *   auto-adapts to portrait (X-dominant), landscape (Y-dominant), and flat
 *   (Z-dominant) mounting orientations without user intervention.
 *
 * Calibration (per-orientation offsets):
 *   Three pairs of tilt offsets are maintained, one per dominant-axis mode.
 *   zero() detects the current dominant axis and updates only that mode's
 *   offsets, leaving the other two modes unchanged.  This means:
 *
 *     - Calibrate portrait  (X-dominant) → corrects Y + Z tilt errors
 *     - Calibrate landscape (Y-dominant) → corrects X + Z tilt errors
 *     - Calibrate flat      (Z-dominant) → corrects X + Y tilt errors
 *
 *   The single Flat button therefore serves all orientations; pressing it
 *   twice (once in each real-world mounting orientation) is sufficient to
 *   calibrate the full installation.  Offsets default to 0.0 on first boot.
 *
 * Smoothing:
 *   N=8 sample sliding-window average on raw X/Y/Z before tilt calculation.
 */
class ImuSensor {
public:
    static constexpr const char* TAG = "ImuSensor";

    ImuSensor() = default;

    /**
     * Attach to the shared I2C bus, configure the QMI8658, and load any
     * saved calibration from NVS.  Logs a warning and disables polling if
     * the chip is not found.
     */
    void init(i2c_master_bus_handle_t bus);

    /**
     * Read one accelerometer sample, update the moving average, compute
     * corrected tilt angles, and write them into `out`.
     * No-op if init() failed.  Safe to call every loop tick (50 ms).
     */
    void poll(display::LevelData& out);

    /**
     * Capture the current tilt as the calibration offset for the current
     * dominant-axis orientation mode, then persist all offsets to NVS.
     * Call while the van (or test surface) is known to be level.
     */
    void zero();

private:
    // ── QMI8658 register helpers ──────────────────────────────────────────
    esp_err_t writeReg(uint8_t reg, uint8_t val);
    esp_err_t readRegs(uint8_t reg, uint8_t* buf, size_t len);

    // ── Calibration helpers ───────────────────────────────────────────────
    void loadCalibration();
    void saveCalibration();

    // ── Moving average (N = 8 samples) ───────────────────────────────────
    static constexpr int AVG_N = 8;

    float bufX_[AVG_N] = {};
    float bufY_[AVG_N] = {};
    float bufZ_[AVG_N] = {};
    float sumX_ {0.f}, sumY_ {0.f}, sumZ_ {0.f};
    int   avgIdx_   {0};
    int   avgCount_ {0};

    // ── Sensor state ──────────────────────────────────────────────────────
    i2c_master_dev_handle_t dev_   {nullptr};
    bool                    ready_ {false};

    // ── Per-orientation tilt offsets (degrees) ────────────────────────────
    // Indexed by dominant-axis mode: 0 = X-dominant, 1 = Y-dominant,
    // 2 = Z-dominant.  Captured by zero() and persisted to NVS.
    // Default 0.0 — no correction until calibrated.
    float offsetTiltX_[3] = {};
    float offsetTiltY_[3] = {};
};

} // namespace app
