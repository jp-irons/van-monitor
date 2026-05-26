#include "ApplicationContext.hpp"

#include "http_types/HttpTypes.hpp"
#include "logger/Logger.hpp"
#include "esp_app_desc.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static logger::Logger log{app::ApplicationContext::TAG};

namespace app {

	ApplicationContext::ApplicationContext(framework::FrameworkContext& fw)
	    : fw_(fw)
	    , appFileTable_()
	    , appFileHandler_("", "index.html", appFileTable_)
	    , activityManager_()
	    , temperatureHandler_(fw.getDevice())
	    , waterLevelSensor_()
	    , appState_{}
	    , statusHandler_(appState_)
	    , calibrateHandler_(appState_)
	    , venusConfigHandler_()
	    , venusClient_()
	    , display_() {
	    log.debug("constructor");
	}

	ApplicationContext::~ApplicationContext() {
	    log.info("destructor");
	}

	void ApplicationContext::start() {
	    log.debug("start");

	    // ── Register app static-file handler ──────────────────────────────────
	    // Mounted at "/" so all paths are looked up verbatim in the app file table.
	    // Framework routes and the framework file handler are still tried first
	    // (or as fallback) — the app handler returns NotFound for anything not in
	    // its table, allowing requests to fall through.
	    fw_.addFileHandler("/", &appFileHandler_);

	    // ── Set the entry point ────────────────────────────────────────────────
	    // Visiting the root URL (/) will redirect here.  Remove or change this
	    // line to fall back to the framework's own management UI (/framework/ui/).
	    fw_.setEntryPoint("/app/ui/");

	    // ── Register app API routes ────────────────────────────────────────────
	    fw_.addRoute(http::HttpMethod::Get,  "/app/api/temperature",  &temperatureHandler_);
	    fw_.addRoute(http::HttpMethod::Get,  "/app/api/status",       &statusHandler_);
	    fw_.addRoute(http::HttpMethod::Post, "/app/api/calibrate",    &calibrateHandler_);
	    fw_.addRoute(http::HttpMethod::Get,  "/app/api/venus/config", &venusConfigHandler_);
	    fw_.addRoute(http::HttpMethod::Post, "/app/api/venus/config", &venusConfigHandler_);

	    // ── Configure pull-based OTA ──────────────────────────────────────────
	    // baseUrl            — GitHub Releases download directory for this repo.
	    //                      OtaPuller appends "/version.txt" (checked first)
	    //                      and "/firmware.bin" (downloaded only if newer).
	    // checkIntervalS     — Seconds between background checks; 0 disables the
	    //                      periodic task (manual / MQTT-triggered checks still
	    //                      work via the firmware UI or checkNow()).
	    // autoUpdateEnabled  — Default auto-update state.  true = checks run
	    //                      automatically; false = disabled until toggled on.
	    //                      When uiSettable=true, a user-persisted NVS value
	    //                      overrides this default after the first toggle.
	    // uiSettable         — When true, the firmware UI exposes an enable/disable
	    //                      toggle and the POST /firmware/autoUpdate API is
	    //                      accepted; the user's choice survives reboots via NVS.
	    //                      When false, autoUpdateEnabled is always authoritative
	    //                      and the toggle is hidden.
	    fw_.setOtaPullConfig({
	        .baseUrl           = "https://github.com/jp-irons/van-monitor/releases/latest/download",
	        .checkIntervalS    = 3600,
	        .autoUpdateEnabled = false,
	        .uiSettable        = true,
	    });

	    // ── Device identity ───────────────────────────────────────────────────────
	    // By default both setters append the last 3 MAC bytes (MacShort) to the
	    // supplied prefix, e.g. "van-monitor-a1b2c3" / "VanMonitor-a1b2c3".
	    // This ensures uniqueness when multiple units share a location.
	    //
	    // To suppress the suffix pass wifi_manager::SuffixPolicy::None, or to use
	    // all 6 MAC bytes pass wifi_manager::SuffixPolicy::MacFull — both require
	    // #include "wifi_manager/WiFiTypes.hpp".
	    fw_.setHostnameConfig("van-monitor", wifi_manager::SuffixPolicy::None);
	    fw_.setApSsidConfig("VanMonitor", wifi_manager::SuffixPolicy::None);
	    fw_.setApPassword("vanmonitor");

	    // ── Start the framework (WiFi, server, OTA, …) ────────────────────────
	    fw_.start();

	    // ── Cache firmware version for system screen ──────────────────────────
	    const esp_app_desc_t* desc = esp_app_get_description();
	    sysFwVer_ = (desc && desc->version[0]) ? desc->version : "---";

	    // ── Start the display (NVS must be ready before this) ─────────────────
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	    display_.start();
	    display_.setActivityCallback([this] { activityManager_.poke(); });
#endif

	    // ── Start activity manager ────────────────────────────────────────────
	    // onActivate:   brighten display.
	    // onDeactivate: dim display + return to dashboard if on another page.
	    // Display calls are guarded so the activity manager works without display hardware.
	    activityManager_.start(
	        60'000,
	        [this] {
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	            display_.brighten();
#endif
	        },
	        [this] {
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	            display_.dim();
	            display_.scheduleReturnToDashboard();
#endif
	        }
	    );

	    // ── Initialise water level ADC ────────────────────────────────────────
	    // Called after fw_.start() so NVS is guaranteed initialised; init()
	    // calls loadCalibrationFromNvs() immediately to prime the cal values.
	    waterLevelSensor_.init();

	    // ── Start Venus OS MQTT client ────────────────────────────────────────
	    // Reads broker_ip / portal_id from NVS; no-op if not yet configured.
	    // The client retries connection automatically once WiFi associates.
	    venusClient_.start();
	}

	void ApplicationContext::loop() {
	    // Optional per-tick work.  The main loop calls this every 50 ms.
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	    display_.loop();
#endif

	    // Poll ADC, update shared state, push to display (when present)
	    waterLevelSensor_.poll(appState_.water, loopTick_);

	    // Venus OS MQTT keepalive pump + pull latest battery values into shared state
	    venusClient_.loop(loopTick_);
	    venusClient_.fillBattery(appState_.battery);

#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	    display_.updateWaterLevel(appState_.water);
	    display_.updateBattery(appState_.battery);
#endif

	    // Push system info to display once per second (every 20 ticks × 50 ms)
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	    if (loopTick_ % 20 == 0) {
	        pushSystemData_();
	    }
#endif

	    loopTick_++;
	}

	void ApplicationContext::pushSystemData_()
	{
	    // ── Wi-Fi STA info ────────────────────────────────────────────────────
	    wifi_ap_record_t ap = {};
	    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
	        sysSSID_ = reinterpret_cast<const char*>(ap.ssid);
	        sysRssi_ = ap.rssi;
	    } else {
	        sysSSID_ = "---";
	        sysRssi_ = 0;
	    }

	    // ── IP address ────────────────────────────────────────────────────────
	    esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
	    if (sta) {
	        esp_netif_ip_info_t ipInfo = {};
	        if (esp_netif_get_ip_info(sta, &ipInfo) == ESP_OK && ipInfo.ip.addr != 0) {
	            char buf[16];
	            esp_ip4addr_ntoa(&ipInfo.ip, buf, sizeof(buf));
	            sysIP_ = buf;
	        } else {
	            sysIP_ = "---";
	        }
	    } else {
	        sysIP_ = "---";
	    }

	    // ── Venus OS status ──────────� ───────────────────────────────────────────
    bool        mqttOk    = venusClient_.isConnected();
    const char* portalId  = venusClient_.portalId();
    bool        venusOk   = mqttOk && portalId[0] != '\0';

    // ── Build and push ────────────────────────────────────────────────────
    uint32_t uptimeS = static_cast<uint32_t>(esp_timer_get_time() / 1'000'000ULL);

    display::SystemData sd = {};
    sd.ssid            = sysSSID_.c_str();
    sd.rssi            = sysRssi_;
    sd.ipAddr          = sysIP_.c_str();
    sd.hostname        = sysHost_.c_str();
    sd.mqttOk          = mqttOk;
    sd.mqttBrokerIp    = venusClient_.brokerIp();
    sd.venusPortalId   = portalId[0] ? portalId : "---";
    sd.venusOk         = venusOk;
    sd.uptimeS         = uptimeS;
    sd.firmwareVersion = sysFwVer_.c_str();
    display_.updateSystem(sd);
}

} // namespace app
