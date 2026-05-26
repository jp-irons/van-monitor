#include "VenusMqttClient.hpp"

#include "logger/Logger.hpp"

#include "nvs_flash.h"
#include "nvs.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>   // strtof

static logger::Logger log{app::VenusMqttClient::TAG};

namespace app {

// ── Destructor ────────────────────────────────────────────────────────────────

VenusMqttClient::~VenusMqttClient() {
    if (client_) {
        esp_mqtt_client_stop(client_);
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
    }
}

// ── start ─────────────────────────────────────────────────────────────────────

void VenusMqttClient::start() {
    if (started_) return;

    // ── Read NVS config ───────────────────────────────────────────────────────
    {
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
            log.info("no venus config in NVS — MQTT not started");
            return;
        }
        size_t len = sizeof(brokerIp_);
        nvs_get_str(h, "broker_ip", brokerIp_, &len);
        len = sizeof(portalId_);
        nvs_get_str(h, "portal_id", portalId_, &len);
<<<<<<< HEAD
        nvs_get_u16(h, "solar_inst1", &solarInst1_);
        nvs_get_u16(h, "solar_inst2", &solarInst2_);
        nvs_close(h);
    }

    if (brokerIp_[0] == '\0') {
        log.info("broker_ip not configured — MQTT not started");
        return;
    }

    // ── Pre-compute topic strings ─────────────────────────────────────────────
    if (portalId_[0] != '\0') {
        snprintf(keepaliveTopic_,   sizeof(keepaliveTopic_),
                 "R/%s/keepalive",                        portalId_);
        snprintf(topicSoc_,         sizeof(topicSoc_),
                 "N/%s/system/0/Dc/Battery/Soc",          portalId_);
        snprintf(topicVoltage_,     sizeof(topicVoltage_),
                 "N/%s/system/0/Dc/Battery/Voltage",      portalId_);
        snprintf(topicCurrent_,     sizeof(topicCurrent_),
                 "N/%s/system/0/Dc/Battery/Current",      portalId_);
        snprintf(topicSolarW_,      sizeof(topicSolarW_),
                 "N/%s/system/0/Dc/Pv/Power",             portalId_);
        snprintf(topicLoadW_,       sizeof(topicLoadW_),
                 "N/%s/system/0/Dc/System/Power",         portalId_);
        snprintf(topicSolarYield1_, sizeof(topicSolarYield1_),
                 "N/%s/solarcharger/%u/Yield/User",       portalId_, solarInst1_);
        if (solarInst2_ != 0) {
            snprintf(topicSolarYield2_, sizeof(topicSolarYield2_),
                     "N/%s/solarcharger/%u/Yield/User",   portalId_, solarInst2_);
        }
        log.info("portal_id=%s  inst1=%u  inst2=%u", portalId_, solarInst1_, solarInst2_);
    } else {
        log.warn("portal_id not configured — keepalive and subscriptions disabled");
    }

    // ── Init esp_mqtt_client ──────────────────────────────────────────────────
    char uri[80];
    snprintf(uri, sizeof(uri), "mqtt://%s:1883", brokerIp_);
    log.info("connecting to %s", uri);

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = uri;
    cfg.session.keepalive  = 60;

    client_ = esp_mqtt_client_init(&cfg);
    if (!client_) {
        log.error("esp_mqtt_client_init failed");
        return;
    }

    esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY,
                                   mqttEventHandler, this);
    esp_err_t err = esp_mqtt_client_start(client_);
    if (err != ESP_OK) {
        log.error("esp_mqtt_client_start failed: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        return;
    }

    started_ = true;
    log.info("MQTT client started");
}

// ── loop ──────────────────────────────────────────────────────────────────────

void VenusMqttClient::loop(uint32_t tick) {
    if (!client_ || !connected_.load()) return;
    if (keepaliveTopic_[0] == '\0') return;

    bool justConn = justConnected_.exchange(false);
    if (justConn || (tick - lastKeepaliveTick_ >= KEEPALIVE_TICKS)) {
        esp_mqtt_client_publish(client_, keepaliveTopic_, "", 0, 0, 0);
        lastKeepaliveTick_ = tick;
        log.info("keepalive → %s", keepaliveTopic_);
    }
}

// ── fillBattery ───────────────────────────────────────────────────────────────

void VenusMqttClient::fillBattery(display::BatteryData& out) const {
    // Individual float reads are single-instruction atomic on Xtensa LX7.
    // See threading note in VenusMqttClient.hpp.
    out.soc           = latestSoc_;
    out.voltage       = latestVoltage_;
    out.current       = latestCurrent_;
    out.solarW        = latestSolarW_;
    out.solarYieldKwh = latestSolarYield1_ + latestSolarYield2_;
    out.loadW         = latestLoadW_;
}

// ── MQTT event handler ────────────────────────────────────────────────────────

void VenusMqttClient::mqttEventHandler(void* args, esp_event_base_t /*base*/,
                                       int32_t eventId, void* eventData) {
    auto* self  = static_cast<VenusMqttClient*>(args);
    auto* event = static_cast<esp_mqtt_event_handle_t>(eventData);
    self->onEvent(event);
    (void)eventId;
}

void VenusMqttClient::onEvent(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {

        case MQTT_EVENT_CONNECTED:
            log.info("connected to broker");
            connected_     = true;
            justConnected_ = true;

            if (topicSoc_[0] != '\0') {
                esp_mqtt_client_subscribe(client_, topicSoc_,         0);
                esp_mqtt_client_subscribe(client_, topicVoltage_,     0);
                esp_mqtt_client_subscribe(client_, topicCurrent_,     0);
                esp_mqtt_client_subscribe(client_, topicSolarW_,      0);
                esp_mqtt_client_subscribe(client_, topicLoadW_,       0);
                esp_mqtt_client_subscribe(client_, topicSolarYield1_, 0);
                if (topicSolarYield2_[0] != '\0')
                    esp_mqtt_client_subscribe(client_, topicSolarYield2_, 0);
                log.info("subscribed to battery + solar topics (inst1=%u inst2=%u)",
                         solarInst1_, solarInst2_);
            }
            break;

        case MQTT_EVENT_DATA: {
            if (event->topic_len == 0 || event->data_len == 0) break;

            float val;
            if (!parseVenusValue(event->data, event->data_len, val)) break;

            auto topicIs = [&](const char* t) -> bool {
                return t[0] != '\0' &&
                       static_cast<int>(strlen(t)) == event->topic_len &&
                       memcmp(t, event->topic, event->topic_len) == 0;
            };

            if      (topicIs(topicSoc_))          { latestSoc_         = val; }
            else if (topicIs(topicVoltage_))       { latestVoltage_     = val; }
            else if (topicIs(topicCurrent_))       { latestCurrent_     = val; }
            else if (topicIs(topicSolarW_))        { latestSolarW_      = val; }
            else if (topicIs(topicLoadW_))         { latestLoadW_       = val; }
            else if (topicIs(topicSolarYield1_))   { latestSolarYield1_ = val; }
            else if (topicIs(topicSolarYield2_))   { latestSolarYield2_ = val; }
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            log.warn("disconnected from broker");
            connected_ = false;
            break;

        case MQTT_EVENT_ERROR:
            log.warn("MQTT error — type %d", (int)event->error_handle->error_type);
            connected_ = false;
            break;

        default:
            break;
    }
}

// ── parseVenusValue ───────────────────────────────────────────────────────────

// Venus OS publishes each topic as:  {"value": 85.2}  or  {"value": null}
// data is NOT null-terminated; use dlen for bounds checking.
bool VenusMqttClient::parseVenusValue(const char* data, int dlen, float& out) {
    for (int i = 0; i <= dlen - 7; ++i) {
        if (memcmp(data + i, "\"value\"", 7) != 0) continue;

        const char* p   = data + i + 7;
        const char* end = data + dlen;

        while (p < end && (*p == ':' || *p == ' ')) ++p;
        if (p >= end) return false;
        if (end - p >= 4 && memcmp(p, "null", 4) == 0) return false;

=======
        nvs_close(h);
    }

    if (brokerIp_[0] == '\0') {
        log.info("broker_ip not configured — MQTT not started");
        return;
    }

    // ── Pre-compute topic strings ─────────────────────────────────────────────
    if (portalId_[0] != '\0') {
        snprintf(keepaliveTopic_,  sizeof(keepaliveTopic_),
                 "R/%s/keepalive",                    portalId_);
        snprintf(topicSoc_,        sizeof(topicSoc_),
                 "N/%s/system/0/Dc/Battery/Soc",      portalId_);
        snprintf(topicVoltage_,    sizeof(topicVoltage_),
                 "N/%s/system/0/Dc/Battery/Voltage",  portalId_);
        snprintf(topicCurrent_,    sizeof(topicCurrent_),
                 "N/%s/system/0/Dc/Battery/Current",  portalId_);
        snprintf(topicSolarW_,     sizeof(topicSolarW_),
                 "N/%s/solarcharger/258/Yield/Power", portalId_);  // VE.Direct instance 258
        snprintf(topicSolarYield_, sizeof(topicSolarYield_),
                 "N/%s/solarcharger/258/Yield/User",  portalId_);  // VE.Direct instance 258
        log.info("portal_id=%s  keepalive: %s", portalId_, keepaliveTopic_);
    } else {
        log.warn("portal_id not configured — keepalive and subscriptions disabled");
    }

    // ── Init esp_mqtt_client ──────────────────────────────────────────────────
    char uri[80];
    snprintf(uri, sizeof(uri), "mqtt://%s:1883", brokerIp_);
    log.info("connecting to %s", uri);

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = uri;
    cfg.session.keepalive  = 60;   // MQTT protocol keepalive (seconds)

    client_ = esp_mqtt_client_init(&cfg);
    if (!client_) {
        log.error("esp_mqtt_client_init failed");
        return;
    }

    esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY,
                                   mqttEventHandler, this);
    esp_err_t err = esp_mqtt_client_start(client_);
    if (err != ESP_OK) {
        log.error("esp_mqtt_client_start failed: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        return;
    }

    started_ = true;
    log.info("MQTT client started");
}

// ── loop ──────────────────────────────────────────────────────────────────────

void VenusMqttClient::loop(uint32_t tick) {
    if (!client_ || !connected_.load()) return;
    if (keepaliveTopic_[0] == '\0') return;

    // Send immediately after each (re)connect, then on the normal interval.
    bool justConn = justConnected_.exchange(false);
    if (justConn || (tick - lastKeepaliveTick_ >= KEEPALIVE_TICKS)) {
        esp_mqtt_client_publish(client_, keepaliveTopic_, "", 0, 0, 0);
        lastKeepaliveTick_ = tick;
        log.info("keepalive → %s", keepaliveTopic_);
    }
}

// ── fillBattery ───────────────────────────────────────────────────────────────

void VenusMqttClient::fillBattery(display::BatteryData& out) const {
    // Individual float reads are single-instruction atomic on Xtensa LX7.
    // See threading note in VenusMqttClient.hpp.
    out.soc           = latestSoc_;
    out.voltage       = latestVoltage_;
    out.current       = latestCurrent_;
    out.solarW        = latestSolarW_;
    out.solarYieldKwh = latestSolarYieldKwh_;
    out.loadW         = 0.0f;  // not yet subscribed
}

// ── MQTT event handler ────────────────────────────────────────────────────────

void VenusMqttClient::mqttEventHandler(void* args, esp_event_base_t /*base*/,
                                       int32_t eventId, void* eventData) {
    auto* self  = static_cast<VenusMqttClient*>(args);
    auto* event = static_cast<esp_mqtt_event_handle_t>(eventData);
    self->onEvent(event);
    (void)eventId;
}

void VenusMqttClient::onEvent(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {

        case MQTT_EVENT_CONNECTED:
            log.info("connected to broker");
            connected_     = true;
            justConnected_ = true;   // signal loop() to send keepalive immediately

            // Subscribe to battery / solar topics (only if portal_id was configured)
            if (topicSoc_[0] != '\0') {
                esp_mqtt_client_subscribe(client_, topicSoc_,        0);
                esp_mqtt_client_subscribe(client_, topicVoltage_,    0);
                esp_mqtt_client_subscribe(client_, topicCurrent_,    0);
                esp_mqtt_client_subscribe(client_, topicSolarW_,     0);
                esp_mqtt_client_subscribe(client_, topicSolarYield_, 0);
                log.info("subscribed to battery + solar topics");
            }
            break;

        case MQTT_EVENT_DATA: {
            if (event->topic_len == 0 || event->data_len == 0) break;

            float val;
            if (!parseVenusValue(event->data, event->data_len, val)) break;

            // Match incoming topic (NOT null-terminated — use topic_len)
            auto topicIs = [&](const char* t) -> bool {
                return static_cast<int>(strlen(t)) == event->topic_len &&
                       memcmp(t, event->topic, event->topic_len) == 0;
            };

            if      (topicIs(topicSoc_))        { latestSoc_           = val; }
            else if (topicIs(topicVoltage_))     { latestVoltage_       = val; }
            else if (topicIs(topicCurrent_))     { latestCurrent_       = val; }
            else if (topicIs(topicSolarW_))      { latestSolarW_        = val; }
            else if (topicIs(topicSolarYield_))  { latestSolarYieldKwh_ = val; }
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            log.warn("disconnected from broker");
            connected_ = false;
            break;

        case MQTT_EVENT_ERROR:
            log.warn("MQTT error — type %d", (int)event->error_handle->error_type);
            connected_ = false;
            break;

        default:
            break;
    }
}

// ── parseVenusValue ───────────────────────────────────────────────────────────

// Venus OS publishes each topic as:  {"value": 85.2}  or  {"value": null}
// This helper extracts the float value without a full JSON parser.
// Returns false for null payloads or parse failures.
// Note: data is NOT null-terminated; use dlen for bounds checking.
bool VenusMqttClient::parseVenusValue(const char* data, int dlen, float& out) {
    // Find "value" key
    for (int i = 0; i <= dlen - 7; ++i) {
        if (memcmp(data + i, "\"value\"", 7) != 0) continue;

        const char* p   = data + i + 7;
        const char* end = data + dlen;

        // Skip ':' and whitespace
        while (p < end && (*p == ':' || *p == ' ')) ++p;
        if (p >= end) return false;

        // Reject null
        if (end - p >= 4 && memcmp(p, "null", 4) == 0) return false;

        // Copy to a null-terminated buffer for strtof
>>>>>>> branch 'development' of https://github.com/jp-irons/van-monitor
        char num[32];
        int  n = static_cast<int>(end - p);
        if (n > 31) n = 31;
        memcpy(num, p, n);
        num[n] = '\0';

        char* endp;
        out = strtof(num, &endp);
        return endp != num;
    }
    return false;
}

} // namespace app
