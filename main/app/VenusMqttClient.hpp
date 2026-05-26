#pragma once

#include "DisplayContext.hpp"
#include "mqtt_client.h"

#include <atomic>
#include <cstdint>

namespace app {

/**
 * VenusMqttClient — manages the ESP-IDF MQTT connection to a Venus OS broker.
 *
 * Steps 2 + 3:
 *   - Reads broker_ip and portal_id from NVS namespace "venus".
 *   - Connects to mqtt://<broker_ip>:1883 (no auth — local network only).
 *   - Sends a keepalive publish to R/<portal_id>/keepalive immediately on
 *     connect and every ~55 s thereafter.
 *
 * Step 4 — subscriptions:
 *   On connect, subscribes to:
 *     N/<portal_id>/system/0/Dc/Battery/Soc      → latestSoc_
 *     N/<portal_id>/system/0/Dc/Battery/Voltage  → latestVoltage_
 *     N/<portal_id>/system/0/Dc/Battery/Current  → latestCurrent_
 *     N/<portal_id>/system/0/Dc/Pv/Power         → latestSolarW_  (system aggregate — all chargers)
 *     N/<portal_id>/solarcharger/<inst1>/Yield/User → latestSolarYield1_
 *     N/<portal_id>/solarcharger/<inst2>/Yield/User → latestSolarYield2_  (if inst2 != 0)
 *
 *   Instance numbers are read from NVS keys solar_inst1 (default 258) and
 *   solar_inst2 (default 0 = disabled).  Both are configurable via the Venus
 *   OS tab in the web UI without recompiling.
 *
 *   Venus publishes each as {"value": <number>} or {"value": null}.
 *
 * Threading:
 *   The esp_mqtt_client event handler runs on the MQTT internal task.
 *   Atomic booleans guard the connection flags.  The latest* float members are
 *   written only from the MQTT task and read only from the main-loop task via
 *   fillBattery().  On Xtensa LX7, 32-bit aligned float accesses are
 *   single-instruction atomic — torn reads are impossible in practice.
 *   These are slow-changing display values; a stale read for one 50 ms tick
 *   is harmless.
 *
 * Usage (ApplicationContext):
 *   start()           — call once after fw_.start() (NVS and WiFi stack ready).
 *   loop(tick)        — call every 50 ms from ApplicationContext::loop().
 *   fillBattery(out)  — call every loop tick to copy latest values into AppState.
 *   isConnected() / portalId() — read from pushSystemData_().
 */
class VenusMqttClient {
public:
    static constexpr const char* TAG = "VenusMqttClient";

    VenusMqttClient() = default;
    ~VenusMqttClient();

    void start();
    void loop(uint32_t tick);

    /**
     * Copy the latest received battery values into out.
     * solarYieldKwh is the sum of both configured charger instances.
     * loadW is not yet subscribed; always 0.
     */
    void fillBattery(display::BatteryData& out) const;

    bool        isConnected() const { return connected_.load(); }
    const char* portalId()    const { return portalId_; }
    const char* brokerIp()    const { return brokerIp_; }

private:
    static constexpr const char* NVS_NS         = "venus";
    static constexpr uint32_t    KEEPALIVE_TICKS = 1100;  // ~55 s at 50 ms/tick

    static void  mqttEventHandler(void* args, esp_event_base_t base,
                                  int32_t eventId, void* eventData);
    void         onEvent(esp_mqtt_event_handle_t event);
    static bool  parseVenusValue(const char* data, int dlen, float& out);

    // ── MQTT client ───────────────────────────────────────────────────────
    esp_mqtt_client_handle_t client_        {nullptr};
    std::atomic<bool>        connected_     {false};
    std::atomic<bool>        justConnected_ {false};

    // ── NVS-persisted config ──────────────────────────────────────────────
    char     brokerIp_   [64] = {};
    char     portalId_   [32] = {};
    uint16_t solarInst1_      {258};  // VE.Direct instance for charger 1
    uint16_t solarInst2_      {0};    // VE.Direct instance for charger 2 (0 = disabled)

    // ── Pre-computed topic strings ────────────────────────────────────────
    char keepaliveTopic_  [48] = {};  // R/<portal_id>/keepalive
    char topicSoc_        [64] = {};  // N/<portal_id>/system/0/Dc/Battery/Soc
    char topicVoltage_    [64] = {};  // N/<portal_id>/system/0/Dc/Battery/Voltage
    char topicCurrent_    [64] = {};  // N/<portal_id>/system/0/Dc/Battery/Current
    char topicSolarW_     [56] = {};  // N/<portal_id>/system/0/Dc/Pv/Power     (system aggregate)
    char topicLoadW_      [64] = {};  // N/<portal_id>/system/0/Dc/System/Power  (DC loads; null on VEBus demos)
    char topicSolarYield1_[64] = {};  // N/<portal_id>/solarcharger/<inst1>/Yield/User
    char topicSolarYield2_[64] = {};  // N/<portal_id>/solarcharger/<inst2>/Yield/User (empty if inst2==0)

    // ── Latest received values (written by MQTT task, read by main loop) ──
    float latestSoc_          {0.0f};
    float latestVoltage_      {0.0f};
    float latestCurrent_      {0.0f};
    float latestSolarW_       {0.0f};
    float latestLoadW_        {0.0f};
    float latestSolarYield1_  {0.0f};
    float latestSolarYield2_  {0.0f};

    uint32_t lastKeepaliveTick_ {0};
    bool     started_           {false};
};

} // namespace app
