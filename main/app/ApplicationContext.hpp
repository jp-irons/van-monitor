#pragma once

#include "ActivityManager.hpp"
#include "AppFileTable.hpp"
#include "AppState.hpp"
#include "CalibrateHandler.hpp"
#include "StatusHandler.hpp"
#include "TemperatureHandler.hpp"
#include "VenusConfigHandler.hpp"
#include "VenusMqttClient.hpp"
#include "WaterLevelSensor.hpp"
#include "ImuSensor.hpp"
#include "DisplayContext.hpp"
#include "framework/FrameworkContext.hpp"
#include "framework_files/EmbeddedFileHandler.hpp"

#include <cstdint>
#include <string>

namespace app {

/**
 * ApplicationContext — the app-side counterpart to FrameworkContext.
 *
 * Responsibilities:
 *  - Own the app's embedded file table and file handler.
 *  - Register app routes and file handlers with the framework before start().
 *  - Set the entry point so the root path redirects to the app's UI.
 *  - Delegate start() / loop() calls.
 *
 * To add app-specific API handlers, declare them as members here, then
 * register them in start() via fw_.addRoute(...).
 */
 class ApplicationContext {
   public:
     static constexpr const char* TAG = "ApplicationContext";

     explicit ApplicationContext(framework::FrameworkContext& fw);
     ~ApplicationContext();

     void start();
     void loop();

   private:
     framework::FrameworkContext& fw_;

     // App embedded file table + handler.
     // appFileTable_ MUST be declared before appFileHandler_ so it is
     // initialised first (appFileHandler_ holds a reference to it).
     AppFileTable                          appFileTable_;
     framework_files::EmbeddedFileHandler  appFileHandler_;

     ActivityManager activityManager_;

     // temperatureHandler_ is declared after fw_ so that fw_.getDevice() is
     // valid when the initialiser list runs.
     TemperatureHandler temperatureHandler_;

     // waterLevelSensor_ has no fw_ dependency at construction time.
     // init() is called in start(), after NVS is ready.
     WaterLevelSensor waterLevelSensor_;

     // appState_ holds the latest sensor snapshot; declare before the handlers
     // that reference it so the object is valid when their constructors run.
     AppState            appState_;
     StatusHandler       statusHandler_;
     CalibrateHandler    calibrateHandler_;
     VenusConfigHandler  venusConfigHandler_;
     VenusMqttClient     venusClient_;

     // Loop tick counter — passed to WaterLevelSensor::poll() so it can
     // throttle NVS calibration refreshes without its own timer.
     uint32_t loopTick_ {0};

     // display_ is declared before imuSensor_ — display_.start() initialises
     // the shared I2C bus; imuSensor_.init() must be called after.
     display::DisplayContext display_;

     // imuSensor_ is declared after display_: init() is called in start() once
     // the I2C bus is live (via display_.getI2cBus()).
     ImuSensor imuSensor_;

     // ── Cached system-screen fields ───────────────────────────────────────
     // Updated once per second in loop(); strings outlive each updateSystem() call.
     std::string sysSSID_  {"---"};
     int         sysRssi_  {0};
     std::string sysIP_    {"---"};
     std::string sysHost_  {"van-monitor"};
     std::string sysFwVer_ {"---"};

     void pushSystemData_();
 };
} // namespace app
