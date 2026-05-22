#pragma once

#include "AppState.hpp"
#include "common/Result.hpp"
#include "http/HttpHandler.hpp"

#include <cstdint>

namespace app {

/**
 * Handles POST /app/api/calibrate.
 *
 * Reads a small JSON body and dispatches one of three actions:
 *
 *   {"action":"setEmpty"}                — save current rawVolts as the empty
 *                                          calibration point (cal_v_empty_b)
 *   {"action":"setFull"}                 — save current rawVolts as the full
 *                                          calibration point (cal_v_full_b)
 *   {"action":"setCapacity","litres":N}  — save tank capacity in litres
 *                                          (tank_litres)
 *
 * NVS namespace and key names are identical to those used by CalibrateScreen,
 * so the web UI and the physical display share calibration state transparently.
 *
 * Responses: {"ok":true} on success, {"error":"..."} with HTTP 4xx/5xx on
 * failure.
 */
class CalibrateHandler : public http::HttpHandler {
public:
    static constexpr const char* TAG = "CalibrateHandler";

    explicit CalibrateHandler(AppState& state);
    ~CalibrateHandler();

    common::Result handle(http::HttpRequest& req, http::HttpResponse& res) override;

private:
    AppState& state_;

    static constexpr const char* NVS_NS = "water_cal";

    common::Result setEmpty(http::HttpResponse& res);
    common::Result setFull(http::HttpResponse& res);
    common::Result setCapacity(uint32_t litres, http::HttpResponse& res);

    static bool saveFloatToNvs(const char* key, float value);
    static bool saveU32ToNvs(const char* key, uint32_t value);
};

} // namespace app
