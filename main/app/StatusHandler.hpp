#pragma once

#include "ActivityManager.hpp"
#include "AppState.hpp"
#include "common/Result.hpp"
#include "http/HttpHandler.hpp"

namespace app {

/**
 * Handles GET /app/api/status.
 *
 * Returns a JSON snapshot of the current sensor readings and system info:
 *
 *   {
 *     "water":   { "pct", "litres", "rawVolts", "rawAdc", "computedM",
 *                  "calVEmpty", "calVFull", "tankLitres" },
 *     "battery": { "soc", "voltage", "current", "solarW", "loadW" },
 *     "system":  { "uptimeS", "firmwareVersion" }
 *   }
 *
 * Calibration points and tank capacity are read from NVS on each request so
 * they reflect the latest values written by CalibrateHandler or CalibrateScreen.
 *
 * Battery fields are zero-stub until Venus OS MQTT integration lands.
 */
class StatusHandler : public http::HttpHandler {
public:
    static constexpr const char* TAG = "StatusHandler";

    StatusHandler(const AppState& state, ActivityManager& activity);
    ~StatusHandler();

    common::Result handle(http::HttpRequest& req, http::HttpResponse& res) override;

private:
    const AppState&  state_;
    ActivityManager& activity_;

    static constexpr const char* NVS_NS = "water_cal";
};

} // namespace app
