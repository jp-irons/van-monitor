#pragma once

#include "common/Result.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

#include <string_view>
#include <cstddef>

namespace app {

/**
 * Handles GET and POST /app/api/venus/config.
 *
 * GET  — Returns the stored Venus OS broker IP and portal ID as JSON:
 *          {"broker_ip":"venus.local","portal_id":""}
 *        If the NVS namespace has not been written yet, defaults are returned
 *        ("venus.local" for broker_ip, "" for portal_id).
 *
 * POST — Accepts {"broker_ip":"...","portal_id":"..."} and persists both
 *        strings to NVS namespace "venus".  broker_ip is required and must be
 *        non-empty.  portal_id may be empty (user has not configured it yet).
 *
 * No MQTT connection logic lives here — this handler is pure config storage,
 * consumed by VenusMqttClient in step 2.
 */
class VenusConfigHandler : public http::HttpHandler {
public:
    static constexpr const char* TAG = "VenusConfigHandler";

    VenusConfigHandler();
    ~VenusConfigHandler();

    common::Result handle(http::HttpRequest& req, http::HttpResponse& res) override;

private:
    static constexpr const char* NVS_NS        = "venus";
    static constexpr size_t      BROKER_IP_MAX  = 64;   // covers FQDNs and IPv4
    static constexpr size_t      PORTAL_ID_MAX  = 32;   // Venus portal IDs are 12 hex chars

    common::Result handleGet(http::HttpResponse& res);
    common::Result handlePost(http::HttpRequest& req, http::HttpResponse& res);

    /**
     * Extract the value of a JSON string field from a flat JSON body.
     * Finds "key":"..." and copies the value into out (NUL-terminated).
     * Returns true if the key was found and the closing quote was reached.
     * Does not handle escape sequences — sufficient for IP addresses and IDs.
     */
    static bool extractStr(std::string_view body, const char* key,
                           char* out, size_t maxLen);
};

} // namespace app
