#include "StatusHandler.hpp"

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_app_desc.h"

#include <cstdio>
#include <cstring>

static logger::Logger log{app::StatusHandler::TAG};

namespace app {

StatusHandler::StatusHandler(const AppState& state)
    : state_(state) {}

StatusHandler::~StatusHandler() = default;

common::Result StatusHandler::handle(http::HttpRequest& /*req*/,
                                     http::HttpResponse& res) {
    // ── Read calibration config from NVS ──────────────────────────────────────
    float    calVEmpty  = 0.60f;
    float    calVFull   = 3.00f;
    uint32_t tankLitres = 200;

    {
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
            uint32_t bits = 0;
            if (nvs_get_u32(h, "cal_v_empty_b", &bits) == ESP_OK)
                memcpy(&calVEmpty, &bits, sizeof(calVEmpty));
            bits = 0;
            if (nvs_get_u32(h, "cal_v_full_b", &bits) == ESP_OK)
                memcpy(&calVFull, &bits, sizeof(calVFull));
            nvs_get_u32(h, "tank_litres", &tankLitres);
            nvs_close(h);
        }
    }

    // ── System info ───────────────────────────────────────────────────────────
    uint32_t uptimeS = (uint32_t)(esp_timer_get_time() / 1'000'000ULL);
    const esp_app_desc_t* desc = esp_app_get_description();
    const char* fwVersion = (desc && desc->version[0]) ? desc->version : "unknown";

    // ── Build JSON ────────────────────────────────────────────────────────────
    char body[640];
    snprintf(body, sizeof(body),
        "{"
          "\"water\":{"
            "\"pct\":%.1f,"
            "\"litres\":%.1f,"
            "\"rawVolts\":%.3f,"
            "\"rawAdc\":%u,"
            "\"computedM\":%.3f,"
            "\"calVEmpty\":%.3f,"
            "\"calVFull\":%.3f,"
            "\"tankLitres\":%lu"
          "},"
          "\"battery\":{"
            "\"soc\":%.1f,"
            "\"voltage\":%.2f,"
            "\"current\":%.2f,"
            "\"solarW\":%.1f,"
            "\"loadW\":%.1f"
          "},"
          "\"system\":{"
            "\"uptimeS\":%lu,"
            "\"firmwareVersion\":\"%s\""
          "}"
        "}",
        state_.water.pct,
        state_.water.litres,
        state_.water.rawVolts,
        (unsigned)state_.water.rawAdc,
        state_.water.computedM,
        calVEmpty,
        calVFull,
        (unsigned long)tankLitres,
        state_.battery.soc,
        state_.battery.voltage,
        state_.battery.current,
        state_.battery.solarW,
        state_.battery.loadW,
        (unsigned long)uptimeS,
        fwVersion
    );

    log.debug("water=%.1f%% battery=%.1f%% uptime=%lus", state_.water.pct, state_.battery.soc, (unsigned long)uptimeS);
    return res.sendJson(body);
}

} // namespace app
