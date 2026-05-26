#include "VenusConfigHandler.hpp"

#include "logger/Logger.hpp"

#include "nvs_flash.h"
#include "nvs.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>   // strtoul

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
    char     brokerIp  [BROKER_IP_MAX] = "venus.local";
    char     portalId  [PORTAL_ID_MAX] = "";
    uint16_t solarInst1                = SOLAR_INST1_DEFAULT;
    uint16_t solarInst2                = SOLAR_INST2_DEFAULT;

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(brokerIp);
        nvs_get_str(h, "broker_ip", brokerIp, &len);
        len = sizeof(portalId);
        nvs_get_str(h, "portal_id", portalId, &len);
        nvs_get_u16(h, "solar_inst1", &solarInst1);
        nvs_get_u16(h, "solar_inst2", &solarInst2);
        nvs_close(h);
    }

    char json[256];
    snprintf(json, sizeof(json),
             "{\"broker_ip\":\"%s\",\"portal_id\":\"%s\","
             "\"solar_inst1\":%u,\"solar_inst2\":%u}",
             brokerIp, portalId, solarInst1, solarInst2);

    log.debug("GET: %s", json);
    return res.sendJson(json);
}

// ── POST ──────────────────────────────────────────────────────────────────────

common::Result VenusConfigHandler::handlePost(http::HttpRequest& req,
                                              http::HttpResponse& res) {
    char bodyBuf[256] = {};
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

    char     brokerIp  [BROKER_IP_MAX] = {};
    char     portalId  [PORTAL_ID_MAX] = {};
    uint16_t solarInst1                = SOLAR_INST1_DEFAULT;
    uint16_t solarInst2                = SOLAR_INST2_DEFAULT;

    if (!extractStr(body, "broker_ip", brokerIp, sizeof(brokerIp)) || brokerIp[0] == '\0')
        return res.sendJsonError(400, "broker_ip required");

    // Optional fields — keep defaults if absent
    extractStr(body, "portal_id", portalId, sizeof(portalId));
    extractUint16(body, "solar_inst1", solarInst1);
    extractUint16(body, "solar_inst2", solarInst2);

    log.info("saving: broker_ip=%s portal_id=%s solar_inst1=%u solar_inst2=%u",
             brokerIp, portalId, solarInst1, solarInst2);

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        log.warn("NVS open failed");
        return res.sendJsonError(500, "NVS open failed");
    }

    esp_err_t err = nvs_set_str(h, "broker_ip",   brokerIp);
    if (err == ESP_OK) err = nvs_set_str(h, "portal_id",   portalId);
    if (err == ESP_OK) err = nvs_set_u16(h, "solar_inst1", solarInst1);
    if (err == ESP_OK) err = nvs_set_u16(h, "solar_inst2", solarInst2);
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
    char needle[48];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);

    auto pos = body.find(needle);
    if (pos == std::string_view::npos) return false;
    pos += strlen(needle);

    size_t len = 0;
    while (pos < body.size() && body[pos] != '"' && len < maxLen - 1)
        out[len++] = body[pos++];
    out[len] = '\0';

    return pos < body.size() && body[pos] == '"';
}

bool VenusConfigHandler::extractUint16(std::string_view body, const char* key,
                                        uint16_t& out) {
    // Find "key": followed by digits (unquoted JSON number)
    char needle[48];
    snprintf(needle, sizeof(needle), "\"%s\":", key);

    auto pos = body.find(needle);
    if (pos == std::string_view::npos) return false;
    pos += strlen(needle);

    // Skip whitespace
    while (pos < body.size() && body[pos] == ' ') ++pos;
    if (pos >= body.size()) return false;

    // Must start with a digit
    if (body[pos] < '0' || body[pos] > '9') return false;

    // Copy digits to a small null-terminated buffer for strtoul
    char num[8];
    size_t n = 0;
    while (pos < body.size() && body[pos] >= '0' && body[pos] <= '9' && n < 7)
        num[n++] = body[pos++];
    num[n] = '\0';

    char* end;
    unsigned long val = strtoul(num, &end, 10);
    if (end == num || val > 65535) return false;

    out = static_cast<uint16_t>(val);
    return true;
}

} // namespace app
