#include "VenusConfigHandler.hpp"

#include "logger/Logger.hpp"

#include "nvs_flash.h"
#include "nvs.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

static logger::Logger log{app::VenusConfigHandler::TAG};

namespace app {

VenusConfigHandler::VenusConfigHandler() = default;
VenusConfigHandler::~VenusConfigHandler() = default;

common::Result VenusConfigHandler::handle(http::HttpRequest& req,
                                          http::HttpResponse& res) {
    if (req.method() == http::HttpMethod::Get)
        return handleGet(res);
    if (req.method() == http::HttpMethod::Post)
        return handlePost(req, res);
    return res.sendJsonError(405, "Method not allowed");
}

// ── GET ───────────────────────────────────────────────────────────────────────

common::Result VenusConfigHandler::handleGet(http::HttpResponse& res) {
    char brokerIp[BROKER_IP_MAX] = "venus.local";
    char portalId[PORTAL_ID_MAX] = "";

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(brokerIp);
        nvs_get_str(h, "broker_ip", brokerIp, &len);
        len = sizeof(portalId);
        nvs_get_str(h, "portal_id", portalId, &len);
        nvs_close(h);
    }
    // If the namespace does not exist yet, defaults remain ("venus.local" / "").

    char json[160];
    snprintf(json, sizeof(json),
             "{\"broker_ip\":\"%s\",\"portal_id\":\"%s\"}",
             brokerIp, portalId);

    log.debug("GET: %s", json);
    return res.sendJson(json);
}

// ── POST ──────────────────────────────────────────────────────────────────────

common::Result VenusConfigHandler::handlePost(http::HttpRequest& req,
                                              http::HttpResponse& res) {
    char bodyBuf[200] = {};
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

    log.debug("POST body: %.*s", (int)body.size(), body.data());

    char brokerIp[BROKER_IP_MAX] = {};
    char portalId[PORTAL_ID_MAX] = {};

    if (!extractStr(body, "broker_ip", brokerIp, sizeof(brokerIp)) || brokerIp[0] == '\0')
        return res.sendJsonError(400, "broker_ip required");

    // portal_id is optional — missing key or empty value both mean "not set yet"
    extractStr(body, "portal_id", portalId, sizeof(portalId));

    log.info("saving: broker_ip=%s portal_id=%s", brokerIp, portalId);

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        log.warn("NVS open failed");
        return res.sendJsonError(500, "NVS open failed");
    }

    esp_err_t err = nvs_set_str(h, "broker_ip", brokerIp);
    if (err == ESP_OK) err = nvs_set_str(h, "portal_id", portalId);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK) {
        log.warn("NVS write failed: %s", esp_err_to_name(err));
        return res.sendJsonError(500, "NVS write failed");
    }

    return res.sendJson("{\"ok\":true}");
}

// ── Helpers ───────────────────────────────────────────────────────────────────

bool VenusConfigHandler::extractStr(std::string_view body, const char* key,
                                    char* out, size_t maxLen) {
    // Build needle: "key":"
    char needle[48];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);

    auto pos = body.find(needle);
    if (pos == std::string_view::npos) return false;
    pos += strlen(needle);

    size_t len = 0;
    while (pos < body.size() && body[pos] != '"' && len < maxLen - 1)
        out[len++] = body[pos++];
    out[len] = '\0';

    // Confirm we stopped at the closing quote, not end-of-body or overflow
    return pos < body.size() && body[pos] == '"';
}

} // namespace app
