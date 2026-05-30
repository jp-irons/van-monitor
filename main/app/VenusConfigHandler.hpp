#pragma once

#include "common/Result.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

#include <string_view>
#include <cstddef>
#include <cstdint>

namespace app {

/**
 * Handles GET and POST /app/api/venus/config.
 *
 * GET  — Returns the stored Venus OS config as JSON:
 *          {"broker_ip":"venus.local","portal_id":"",
 *           "solar_inst1":258,"solar_inst2":0}
 *        Defaults are returned if NVS has not been written yet.
 *
 * POST — Accepts the same four fields and persists them to NVS namespace
 *        "venus".  broker_ip is required and must be non-empty.
 *        portal_id may be empty.  solar_inst1 defaults to 258 if omitted.
 *        solar_inst2 defaults to 0 (disabled) if omitted.
 *
 * solar_inst1 / solar_inst2 are the VE.Direct device instance numbers for the
 * solar charger(s); used by VenusMqttClient to subscribe to Yield/User topics.
 * solar_inst2 == 0 means "no second charger configured".
 *
 * No MQTT connection logic lives here — this handler is pure config storage.
 */
class VenusConfigHandler : public http::HttpHandler {
public:
    static constexpr const char* TAG = "VenusConfigHandler";

    VenusConfigHandler();
    ~VenusConfigHandler();

    common::Result handle(http::HttpRequest& req, http::HttpResponse& res) override;

    // ── NVS defaults ─────────────────────────────────────────────────────────
    static constexpr uint16_t SOLAR_INST1_DEFAULT = 288;
    static constexpr uint16_t SOLAR_INST2_DEFAULT = 0;

private:
    static constexpr const char* NVS_NS        = "venus";
    static constexpr size_t      BROKER_IP_MAX  = 64;
    static constexpr size_t      PORTAL_ID_MAX  = 32;

    common::Result handleGet(http::HttpResponse& res);
    common::Result handlePost(http::HttpRequest& req, http::HttpResponse& res);

    /** Extract a JSON string field value — finds "key":"..." */
    static bool extractStr(std::string_view body, const char* key,
                           char* out, size_t maxLen);

    /** Extract a JSON numeric (uint16) field value — finds "key":<digits> */
    static bool extractUint16(std::string_view body, const char* key,
                               uint16_t& out);
};

} // namespace app
