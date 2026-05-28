#pragma once

#include "DisplayContext.hpp"

namespace app {

/**
 * AppState — mutable sensor snapshot shared between ApplicationContext::loop()
 * and the HTTP API handlers (StatusHandler, CalibrateHandler).
 *
 * Owned by ApplicationContext.  Updated each loop() tick once the ADC and
 * Venus OS MQTT are wired up.  Zero-initialised defaults are safe: the SPA
 * renders "—" for any field that is still at its zero value.
 *
 * Handlers hold references into this struct; no locking is required because
 * all writes happen on the main-loop task and the HTTP server tasks only read
 * (StatusHandler) or write NVS + return (CalibrateHandler, which also updates
 * water.rawVolts indirectly via the ADC path in loop()).
 */
struct AppState {
    display::WaterData   water   = {};   // updated each loop() once ADC is wired
    display::BatteryData battery = {};   // stub zeros until Venus OS MQTT lands
    display::LevelData   level   = {};   // updated each loop() by ImuSensor::poll()
};

} // namespace app
