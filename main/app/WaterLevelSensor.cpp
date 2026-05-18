#include "WaterLevelSensor.hpp"

#include "logger/Logger.hpp"

#include "nvs_flash.h"
#include "nvs.h"

#include <algorithm>
#include <cstring>

static logger::Logger log{app::WaterLevelSensor::TAG};

namespace app {

WaterLevelSensor::WaterLevelSensor() = default;

WaterLevelSensor::~WaterLevelSensor() {
    if (caliOk_) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(adcCali_);
#endif
        caliOk_ = false;
    }
    if (ready_) {
        adc_oneshot_del_unit(adcUnit_);
        ready_ = false;
    }
}

// ── init ──────────────────────────────────────────────────────────────────────

bool WaterLevelSensor::init() {
    // ── ADC unit ─────────────────────────────────────────────────────────
    adc_oneshot_unit_init_cfg_t unitCfg = {};
    unitCfg.unit_id  = UNIT;
    unitCfg.ulp_mode = ADC_ULP_MODE_DISABLE;

    esp_err_t err = adc_oneshot_new_unit(&unitCfg, &adcUnit_);
    if (err != ESP_OK) {
        log.error("adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
        return false;
    }

    // ── Channel config ────────────────────────────────────────────────────
    adc_oneshot_chan_cfg_t chanCfg = {};
    chanCfg.atten    = ATTENUATION;
    chanCfg.bitwidth = ADC_BITWIDTH_DEFAULT;

    err = adc_oneshot_config_channel(adcUnit_, CHANNEL, &chanCfg);
    if (err != ESP_OK) {
        log.error("adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
        adc_oneshot_del_unit(adcUnit_);
        return false;
    }

    // ── Hardware calibration (curve fitting — ESP32-S3 native) ───────────
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t caliCfg = {};
    caliCfg.unit_id  = UNIT;
    caliCfg.chan     = CHANNEL;
    caliCfg.atten    = ATTENUATION;
    caliCfg.bitwidth = ADC_BITWIDTH_DEFAULT;

    err = adc_cali_create_scheme_curve_fitting(&caliCfg, &adcCali_);
    if (err == ESP_OK) {
        caliOk_ = true;
        log.info("ADC hw calibration ready (curve fitting)");
    } else {
        log.warn("ADC hw calibration unavailable (%s); using linear fallback",
                 esp_err_to_name(err));
    }
#else
    log.warn("ADC curve-fitting not supported on this target; using linear fallback");
#endif

    ready_ = true;

    // Load calibration immediately — NVS is guaranteed ready by the time
    // init() is called (ApplicationContext::start() calls it after fw_.start()).
    loadCalibrationFromNvs();

    log.info("init ok — ADC1 ch%d, atten=%d dB", (int)CHANNEL, 12);
    return true;
}

// ── poll ──────────────────────────────────────────────────────────────────────

void WaterLevelSensor::poll(display::WaterData& out, uint32_t tick) {
    if (!ready_) return;

    // Refresh 2-point NVS calibration periodically
    if (tick - lastCalTick_ >= CAL_REFRESH_TICKS) {
        loadCalibrationFromNvs();
        lastCalTick_ = tick;
    }

    int raw = sampleAdc();
    if (raw < 0) {
        log.warn("ADC sample failed");
        return;
    }

    float volts = adcToVoltage(raw);

    // IIR low-pass — seed on first reading, blend thereafter
    if (smoothedVolts_ < 0.0f) {
        smoothedVolts_ = volts;
    } else {
        smoothedVolts_ = IIR_ALPHA * smoothedVolts_ + (1.0f - IIR_ALPHA) * volts;
    }

    float span   = calVFull_ - calVEmpty_;
    float levelM = (span > 0.01f) ? (smoothedVolts_ - calVEmpty_) / span * 2.0f : 0.0f;
    levelM = std::max(0.0f, std::min(2.0f, levelM));

    out.rawAdc    = (uint16_t)raw;
    out.rawVolts  = smoothedVolts_;
    out.computedM = levelM;
    out.pct       = levelM / 2.0f * 100.0f;
    out.litres    = levelM / 2.0f * static_cast<float>(tankLitres_);

    log.debug("raw=%d  raw=%.3fV  smooth=%.3fV  %.3fm  %.1f%%  %.1fL",
              raw, volts, smoothedVolts_, levelM, out.pct, out.litres);
}

// ── sampleAdc ─────────────────────────────────────────────────────────────────

int WaterLevelSensor::sampleAdc() {
    int samples[SAMPLE_COUNT];

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        esp_err_t err = adc_oneshot_read(adcUnit_, CHANNEL, &samples[i]);
        if (err != ESP_OK) {
            log.warn("adc_oneshot_read[%d] failed: %s", i, esp_err_to_name(err));
            return -1;
        }
    }

    std::sort(samples, samples + SAMPLE_COUNT);

    // Average the middle (SAMPLE_COUNT - 2*DROP_EACH) readings
    int sum   = 0;
    int count = SAMPLE_COUNT - 2 * DROP_EACH;
    for (int i = DROP_EACH; i < SAMPLE_COUNT - DROP_EACH; i++) {
        sum += samples[i];
    }
    return sum / count;
}

// ── adcToVoltage ──────────────────────────────────────────────────────────────

float WaterLevelSensor::adcToVoltage(int raw) const {
    if (caliOk_) {
        int mV = 0;
        esp_err_t err = adc_cali_raw_to_voltage(adcCali_, raw, &mV);
        if (err == ESP_OK) {
            return static_cast<float>(mV) / 1000.0f;
        }
        log.warn("adc_cali_raw_to_voltage failed: %s", esp_err_to_name(err));
    }
    // Linear fallback: 12-bit ADC, ~3.3 V full-scale at 12 dB atten
    return static_cast<float>(raw) / 4095.0f * 3.3f;
}

// ── loadCalibrationFromNvs ────────────────────────────────────────────────────

void WaterLevelSensor::loadCalibrationFromNvs() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        // Namespace may not exist yet if calibration has never been saved;
        // defaults (0.60 V / 3.00 V / 200 L) remain in place.
        return;
    }

    uint32_t bits = 0;
    if (nvs_get_u32(h, "cal_v_empty_b", &bits) == ESP_OK) {
        memcpy(&calVEmpty_, &bits, sizeof(calVEmpty_));
    }
    bits = 0;
    if (nvs_get_u32(h, "cal_v_full_b", &bits) == ESP_OK) {
        memcpy(&calVFull_, &bits, sizeof(calVFull_));
    }
    nvs_get_u32(h, "tank_litres", &tankLitres_);
    nvs_close(h);

    log.debug("cal loaded: empty=%.3fV full=%.3fV tank=%luL",
              calVEmpty_, calVFull_, (unsigned long)tankLitres_);
}

} // namespace app
