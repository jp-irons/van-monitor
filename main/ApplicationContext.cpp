#include "ApplicationContext.hpp"

#include "http_types/HttpTypes.hpp"
#include "logger/Logger.hpp"
#include "esp_wifi.h"

static logger::Logger log{app::ApplicationContext::TAG};

namespace app {

	ApplicationContext::ApplicationContext(framework::FrameworkContext& fw)
	    : fw_(fw)
	    , appFileTable_()
	    , appFileHandler_("", "index.html", appFileTable_)
	    , activityManager_()
	    , temperatureHandler_(fw.getDevice(), activityManager_)
	    , waterLevelSensor_()
	    , appState_{}
	    , statusHandler_(appState_, activityManager_)
	    , calibrateHandler_(appState_, activityManager_)
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
	    fw_.addRoute(http::HttpMethod::Get,  "/app/api/temperature", &temperatureHandler_);
	    fw_.addRoute(http::HttpMethod::Get,  "/app/api/status",      &statusHandler_);
	    fw_.addRoute(http::HttpMethod::Post, "/app/api/calibrate",   &calibrateHandler_);

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

	    // ── Start the display (NVS must be ready before this) ─────────────────
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	    display_.start();
	    display_.setActivityCallback([this] { activityManager_.poke(); });
#endif

	    // ── Start activity manager ────────────────────────────────────────────
	    // onActivate:   brighten display, restore minimum modem sleep (fast Wi-Fi).
	    // onDeactivate: dim display, switch to maximum modem sleep (low power).
	    // WiFi PS switching is unconditional; display calls are guarded so the
	    // activity manager still works correctly without display hardware.
	    activityManager_.start(
	        60'000,
	        [this] {
	            esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	            display_.brighten();
#endif
	        },
	        [this] {
	            esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	            display_.dim();
#endif
	        }
	    );

	    // ── Initialise water level ADC ────────────────────────────────────────
	    // Called after fw_.start() so NVS is guaranteed initialised; init()
	    // calls loadCalibrationFromNvs() immediately to prime the cal values.
	    waterLevelSensor_.init();
	}

	void ApplicationContext::loop() {
	    // Optional per-tick work.  The main loop calls this every 50 ms.
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	    display_.loop();
#endif

	    // Poll ADC, update shared state, push to display (when present)
	    waterLevelSensor_.poll(appState_.water, loopTick_);
#if CONFIG_VAN_MONITOR_DISPLAY_ENABLED
	    display_.updateWaterLevel(appState_.water);
#endif

	    // TODO: receive Venus OS MQTT data, push to display_.updateBattery() / updateSystem()

	    loopTick_++;
	}
} // namespace app
