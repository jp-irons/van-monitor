#include "ImuSensor.hpp"

#include "logger/Logger.hpp"
#include "nvs.h"

#include <cmath>

namespace app {

// Declared inside namespace app so unqualified lookup finds this before
// double log(double) from <cmath>.
static logger::Logger log{ImuSensor::TAG};

// ── QMI8658A constants ────────────────────────────────────────────────────────

// 7-bit I2C address — probed at runtime: SA0 low → 0x6A, SA0 high → 0x6B.
static constexpr uint32_t QMI8658_I2C_HZ = 400'000;
static constexpr int      I2C_TIMEOUT_MS = 20;

// Registers
static constexpr uint8_t REG_WHO_AM_I = 0x00;
static constexpr uint8_t REG_CTRL1    = 0x02;  // interface config (endian, AI)
static constexpr uint8_t REG_CTRL2    = 0x03;  // accelerometer config
static constexpr uint8_t REG_CTRL7    = 0x08;  // sensor enable
static constexpr uint8_t REG_ACCEL_X  = 0x35;  // first of 6 accel bytes (X_L..Z_H)

// CTRL1: ADDR_AI=1 (burst reads), BE=0 (little-endian) → 0x40
// Power-on default 0x20 has BE=1 and ADDR_AI=0; must be overridden first.
static constexpr uint8_t CTRL1_VAL = 0x40;
// CTRL2: aFS[6:4]=000 (±2 g), aODR[3:0]=0110 (125 Hz) → 0x06
static constexpr uint8_t CTRL2_VAL = 0x06;
// CTRL7: aEN=1 (enable accelerometer) → 0x01
static constexpr uint8_t CTRL7_VAL = 0x01;

// Sensitivity for ±2 g range: 16384 LSB/g
static constexpr float SENSITIVITY = 16384.f;

// ── NVS ───────────────────────────────────────────────────────────────────────

static const char* NVS_NS = "imu_cal";

// ── Dominant-axis helper ──────────────────────────────────────────────────────
//
// Given a normalised gravity vector (gx, gy, gz), identifies which axis
// carries the most of gravity (dominant axis) and computes the uncorrected
// tilt angles in degrees using the other two.
//
// fabsf() on the dominant-axis component prevents atan2f(0, −1) = 180° when
// gravity is on the negative side of that axis.
//
// Returns:
//   0 → X-dominant (portrait)    tiltX = atan2(gy, |gx|)  tiltY = −atan2(gz, |gx|)
//   1 → Y-dominant (landscape)   tiltX = atan2(gz, |gy|)  tiltY = −atan2(gx, |gy|)
//   2 → Z-dominant (flat)        tiltX = atan2(gy, |gz|)  tiltY = −atan2(gx, |gz|)

static int dominantMode(float gx, float gy, float gz,
                        float& tiltX, float& tiltY)
{
    float absX = fabsf(gx), absY = fabsf(gy), absZ = fabsf(gz);

    if (absY >= absX && absY >= absZ) {
        // Y-dominant (landscape)
        tiltX =  atan2f(gz, absY) * (180.f / M_PI);
        tiltY = -atan2f(gx, absY) * (180.f / M_PI);
        return 1;
    } else if (absX >= absY && absX >= absZ) {
        // X-dominant (portrait)
        tiltX =  atan2f(gy, absX) * (180.f / M_PI);
        tiltY = -atan2f(gz, absX) * (180.f / M_PI);
        return 0;
    } else {
        // Z-dominant (flat)
        tiltX =  atan2f(gy, absZ) * (180.f / M_PI);
        tiltY = -atan2f(gx, absZ) * (180.f / M_PI);
        return 2;
    }
}

// ── init ──────────────────────────────────────────────────────────────────────

void ImuSensor::init(i2c_master_bus_handle_t bus) {
    log.debug("init");

    // Auto-detect I2C address
    static constexpr uint8_t PROBE_ADDRS[] = {0x6A, 0x6B};
    uint8_t foundAddr = 0;
    for (uint8_t a : PROBE_ADDRS) {
        if (i2c_master_probe(bus, a, I2C_TIMEOUT_MS) == ESP_OK) {
            foundAddr = a;
            break;
        }
    }
    if (foundAddr == 0) {
        log.warn("QMI8658 not found on I2C bus (tried 0x6A and 0x6B)");
        return;
    }
    log.info("QMI8658 found at 0x%02X", foundAddr);

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = foundAddr;
    dev_cfg.scl_speed_hz    = QMI8658_I2C_HZ;

    if (i2c_master_bus_add_device(bus, &dev_cfg, &dev_) != ESP_OK) {
        log.warn("failed to add QMI8658 to I2C bus");
        return;
    }

    // Must set CTRL1 first: fixes BE=1 and ADDR_AI=0 power-on defaults
    // which corrupt all multi-byte reads.
    if (writeReg(REG_CTRL1, CTRL1_VAL) != ESP_OK) {
        log.warn("CTRL1 write failed");
        return;
    }

    uint8_t who = 0;
    if (readRegs(REG_WHO_AM_I, &who, 1) != ESP_OK) {
        log.warn("WHO_AM_I read failed");
        return;
    }
    log.info("WHO_AM_I = 0x%02X", who);

    if (writeReg(REG_CTRL2, CTRL2_VAL) != ESP_OK) {
        log.warn("CTRL2 write failed");
        return;
    }
    if (writeReg(REG_CTRL7, CTRL7_VAL) != ESP_OK) {
        log.warn("CTRL7 write failed");
        return;
    }

    ready_ = true;
    log.info("QMI8658 ready");

    loadCalibration();
}

// ── poll ──────────────────────────────────────────────────────────────────────

void ImuSensor::poll(display::LevelData& out) {
    if (!ready_) return;

    // Read 6 raw bytes: AX_L, AX_H, AY_L, AY_H, AZ_L, AZ_H
    uint8_t raw[6] = {};
    if (readRegs(REG_ACCEL_X, raw, 6) != ESP_OK) {
        log.warn("accel burst read failed");
        return;
    }

    auto toInt16 = [](uint8_t lo, uint8_t hi) -> int16_t {
        return static_cast<int16_t>((static_cast<uint16_t>(hi) << 8) | lo);
    };
    float ax = toInt16(raw[0], raw[1]) / SENSITIVITY;
    float ay = toInt16(raw[2], raw[3]) / SENSITIVITY;
    float az = toInt16(raw[4], raw[5]) / SENSITIVITY;

    // ── Sliding-window moving average ─────────────────────────────────────
    sumX_ -= bufX_[avgIdx_];  bufX_[avgIdx_] = ax;  sumX_ += ax;
    sumY_ -= bufY_[avgIdx_];  bufY_[avgIdx_] = ay;  sumY_ += ay;
    sumZ_ -= bufZ_[avgIdx_];  bufZ_[avgIdx_] = az;  sumZ_ += az;
    avgIdx_ = (avgIdx_ + 1) % AVG_N;
    if (avgCount_ < AVG_N) ++avgCount_;

    float sax = sumX_ / avgCount_;
    float say = sumY_ / avgCount_;
    float saz = sumZ_ / avgCount_;

    // ── Normalise ─────────────────────────────────────────────────────────
    float mag = sqrtf(sax*sax + say*say + saz*saz);
    if (mag < 0.1f) return;   // sensor data bad — keep last output
    float gx = sax / mag;
    float gy = say / mag;
    float gz = saz / mag;

    // ── Dominant-axis tilt + per-orientation offset correction ────────────
    //
    // dominantMode() auto-detects portrait/landscape/flat and returns the
    // uncorrected tilt.  Subtract the stored offset for that orientation mode
    // (0.0 until zero() has been called for that orientation).
    float tiltX, tiltY;
    int mode = dominantMode(gx, gy, gz, tiltX, tiltY);

    // ── Two-point cal: cancel stale pending flags ─────────────────────────
    // If the user drifted into a different orientation between presses, clear
    // any pending flag for the other mode(s) without saving.
    for (int m = 0; m < 3; ++m) {
        if (m != mode && awaitingPos2_[m]) {
            log.info("poll(): orientation changed — cancelling pending cal for mode %d", m);
            awaitingPos2_[m] = false;
        }
    }

    // ── Two-point cal: auto-save on timeout ───────────────────────────────
    // If the user never performed the flip within FLIP_TIMEOUT_MS, fall back
    // to saving the single pos1 reading (same result as old single-press cal).
    if (awaitingPos2_[mode] &&
        (lv_tick_elaps(pos1TimestampMs_[mode]) >= FLIP_TIMEOUT_MS)) {
        log.info("poll(): flip timeout — saving single-point cal for mode %d  "
                 "offsetX=%.2f°  offsetY=%.2f°",
                 mode, pos1TiltX_[mode], pos1TiltY_[mode]);
        offsetTiltX_[mode] = pos1TiltX_[mode];
        offsetTiltY_[mode] = pos1TiltY_[mode];
        awaitingPos2_[mode] = false;
        saveCalibration();
    }

    out.tiltX        = tiltX - offsetTiltX_[mode];
    out.tiltY        = tiltY - offsetTiltY_[mode];
    out.awaitingFlip = awaitingPos2_[0] || awaitingPos2_[1] || awaitingPos2_[2];
}

// ── zero ─────────────────────────────────────────────────────────────────────

void ImuSensor::zero() {
    if (!ready_) {
        log.warn("zero(): sensor not ready");
        return;
    }
    if (avgCount_ == 0) {
        log.warn("zero(): no samples yet");
        return;
    }

    float sax = sumX_ / avgCount_;
    float say = sumY_ / avgCount_;
    float saz = sumZ_ / avgCount_;
    float mag = sqrtf(sax*sax + say*say + saz*saz);
    if (mag < 0.1f) {
        log.warn("zero(): gravity magnitude too small (%.3f) — ignoring", mag);
        return;
    }
    float gx = sax / mag;
    float gy = say / mag;
    float gz = saz / mag;

    float tiltX, tiltY;
    int mode = dominantMode(gx, gy, gz, tiltX, tiltY);

    static const char* modeNames[] = {
        "X-dominant (portrait)",
        "Y-dominant (landscape)",
        "Z-dominant (flat)"
    };

    // Cancel any pending flag from a different orientation mode (shouldn't
    // normally happen — poll() clears them, but be defensive here too).
    for (int m = 0; m < 3; ++m) {
        if (m != mode && awaitingPos2_[m]) {
            log.info("zero(): cancelling stale pending cal for mode %d", m);
            awaitingPos2_[m] = false;
        }
    }

    if (awaitingPos2_[mode]) {
        // ── Second press ──────────────────────────────────────────────────
        // Check timeout — if expired treat this as a fresh first press.
        if (lv_tick_elaps(pos1TimestampMs_[mode]) >= FLIP_TIMEOUT_MS) {
            log.info("zero(): timeout on second press — restarting as first press");
            awaitingPos2_[mode] = false;
            // fall through to first-press path below
        } else {
            // Average pos1 and pos2 to extract the true sensor offset,
            // eliminating any surface tilt: offset = (R1 + R2) / 2
            offsetTiltX_[mode] = (pos1TiltX_[mode] + tiltX) / 2.0f;
            offsetTiltY_[mode] = (pos1TiltY_[mode] + tiltY) / 2.0f;
            awaitingPos2_[mode] = false;
            log.info("zero(): two-point cal complete  %s  offsetX=%.2f°  offsetY=%.2f°",
                     modeNames[mode], offsetTiltX_[mode], offsetTiltY_[mode]);
            saveCalibration();
            return;
        }
    }

    // ── First press ───────────────────────────────────────────────────────
    // Store pos1 and wait for the user to flip and press again.
    pos1TiltX_[mode]       = tiltX;
    pos1TiltY_[mode]       = tiltY;
    pos1TimestampMs_[mode] = lv_tick_get();
    awaitingPos2_[mode]    = true;
    log.info("zero(): pos1 stored  %s  tiltX=%.2f°  tiltY=%.2f°  — flip 180° and hold Zero",
             modeNames[mode], tiltX, tiltY);
}

// ── loadCalibration ───────────────────────────────────────────────────────────

void ImuSensor::loadCalibration() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        log.info("no saved tilt calibration — offsets default to 0");
        return;
    }

    // Three orientation modes; each stores two float offsets bit-cast to uint32.
    // Keys: "ox_xd"/"oy_xd" (mode 0, X-dominant)
    //       "ox_yd"/"oy_yd" (mode 1, Y-dominant)
    //       "ox_zd"/"oy_zd" (mode 2, Z-dominant)
    struct { const char* kx; const char* ky; int idx; } entries[] = {
        {"ox_xd", "oy_xd", 0},
        {"ox_yd", "oy_yd", 1},
        {"ox_zd", "oy_zd", 2},
    };

    for (auto& e : entries) {
        uint32_t bx = 0, by = 0;
        if (nvs_get_u32(h, e.kx, &bx) == ESP_OK &&
            nvs_get_u32(h, e.ky, &by) == ESP_OK) {
            offsetTiltX_[e.idx] = *reinterpret_cast<float*>(&bx);
            offsetTiltY_[e.idx] = *reinterpret_cast<float*>(&by);
            log.info("cal loaded [mode %d]: offsetX=%.2f°  offsetY=%.2f°",
                     e.idx, offsetTiltX_[e.idx], offsetTiltY_[e.idx]);
        }
    }

    nvs_close(h);
}

// ── saveCalibration ───────────────────────────────────────────────────────────

void ImuSensor::saveCalibration() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        log.warn("saveCalibration(): NVS open failed");
        return;
    }

    struct { const char* kx; const char* ky; int idx; } entries[] = {
        {"ox_xd", "oy_xd", 0},
        {"ox_yd", "oy_yd", 1},
        {"ox_zd", "oy_zd", 2},
    };

    for (auto& e : entries) {
        uint32_t bx = *reinterpret_cast<uint32_t*>(&offsetTiltX_[e.idx]);
        uint32_t by = *reinterpret_cast<uint32_t*>(&offsetTiltY_[e.idx]);
        nvs_set_u32(h, e.kx, bx);
        nvs_set_u32(h, e.ky, by);
    }

    nvs_commit(h);
    nvs_close(h);
    log.info("tilt calibration saved");
}

// ── I2C helpers ───────────────────────────────────────────────────────────────

esp_err_t ImuSensor::writeReg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev_, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

esp_err_t ImuSensor::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
    return i2c_master_transmit_receive(dev_, &reg, 1, buf, len, I2C_TIMEOUT_MS);
}

} // namespace app
