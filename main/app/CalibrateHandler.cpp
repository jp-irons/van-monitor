#include "CalibrateHandler.hpp"

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"

#include "nvs_flash.h"
#include "nvs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

static logger::Logger log{app::CalibrateHandler::TAG};

namespace app {

CalibrateHandler::CalibrateHandler(AppState& state, ActivityManager& activity)
    : state_(state)
    , activity_(activity) {}

CalibrateHandler::~CalibrateHandler() = default;

common::Result CalibrateHandler::handle(http::HttpRequest& req,
                                        http::HttpResponse& res) {
    activity_.poke();

    // ── Read body ─────────────────────────────────────────────────────────────
    // Body is small (<80 bytes); attempt to use the pre-buffered view first,
    // then fall back to a single receiveChunk call.
    char bodyBuf[128] = {};
    std::string_view body = req.body();

    if (body.empty() && req.contentLength() > 0) {
        size_t toRead = std::min(req.contentLength(), sizeof(bodyBuf) - 1);
        int n = req.receiveChunk(bodyBuf, toRead);
        if (n <= 0) {
            log.warn("failed to read body (n=%d)", n);
            return res.sendJsonError(400, "Failed to read body");
        }
        body = std::string_view(bodyBuf, (size_t)n);
    }

    log.debug("body: %.*s", (int)body.size(), body.data());

    // ── Dispatch on action ────────────────────────────────────────────────────
    if (body.find("setEmpty") != std::string_view::npos)
        return setEmpty(res);

    if (body.find("setFull") != std::string_view::npos)
        return setFull(res);

    if (body.find("setCapacity") != std::string_view::npos) {
        auto pos = body.find("\"litres\":");
        if (pos == std::string_view::npos)
            return res.sendJsonError(400, "Missing litres");
        uint32_t litres = (uint32_t)atoi(body.data() + pos + 9);
        if (litres < 10 || litres > 10000)
            return res.sendJsonError(400, "Invalid litres (range 10-10000)");
        return setCapacity(litres, res);
    }

    return res.sendJsonError(400, "Unknown action");
}

// ── Action implementations ────────────────────────────────────────────────────

common::Result CalibrateHandler::setEmpty(http::HttpResponse& res) {
    float v = state_.water.rawVolts;
    log.info("setEmpty: %.3f V", v);
    if (!saveFloatToNvs("cal_v_empty_b", v))
        return res.sendJsonError(500, "NVS write failed");
    return res.sendJson("{\"ok\":true}");
}

common::Result CalibrateHandler::setFull(http::HttpResponse& res) {
    float v = state_.water.rawVolts;
    log.info("setFull: %.3f V", v);
    if (!saveFloatToNvs("cal_v_full_b", v))
        return res.sendJsonError(500, "NVS write failed");
    return res.sendJson("{\"ok\":true}");
}

common::Result CalibrateHandler::setCapacity(uint32_t litres,
                                             http::HttpResponse& res) {
    log.info("setCapacity: %lu L", (unsigned long)litres);
    if (!saveU32ToNvs("tank_litres", litres))
        return res.sendJsonError(500, "NVS write failed");
    return res.sendJson("{\"ok\":true}");
}

// ── NVS helpers ───────────────────────────────────────────────────────────────

bool CalibrateHandler::saveFloatToNvs(const char* key, float value) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        log.warn("NVS open failed");
        return false;
    }
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    esp_err_t err = nvs_set_u32(h, key, bits);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

bool CalibrateHandler::saveU32ToNvs(const char* key, uint32_t value) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        log.warn("NVS open failed");
        return false;
    }
    esp_err_t err = nvs_set_u32(h, key, value);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

} // namespace app
