#pragma once

#include "esp_timer.h"

#include <atomic>
#include <cstdint>
#include <functional>

namespace app {

/**
 * ActivityManager — active / inactive inactivity state machine.
 *
 * Call start() once (after the event loop is running) to arm the timer.
 * Call poke() on any user-initiated event (touch, HTTP request, …).
 *
 *   Active  ──(timeout)──►  Inactive  ──(poke)──►  Active
 *
 * Callbacks:
 *   onActivate   — fired when transitioning Inactive → Active (not on start).
 *   onDeactivate — fired when the inactivity timer expires.
 *
 * Threading:
 *   poke() is safe to call from any FreeRTOS task (LVGL task, HTTP server task).
 *   Callbacks are invoked from the esp_timer task (onDeactivate) or from the
 *   calling task (onActivate, inside poke()).  Keep callbacks short and
 *   non-blocking.  active_ is std::atomic so reads are always consistent.
 *
 * Note: there is a benign race window between the timer firing and a concurrent
 * poke() — the consequence is at most a single spurious activate/deactivate
 * pair.  For a display-dimming use case this is acceptable.
 */
class ActivityManager {
public:
    static constexpr const char* TAG = "ActivityManager";

    ActivityManager();
    ~ActivityManager();

    /**
     * Arm the inactivity timer.
     *
     * @param timeoutMs   Milliseconds of inactivity before onDeactivate fires.
     * @param onActivate  Called when poke() transitions Inactive → Active.
     * @param onDeactivate Called when the timer expires (transitions Active → Inactive).
     */
    void start(uint32_t timeoutMs,
               std::function<void()> onActivate,
               std::function<void()> onDeactivate);

    /**
     * Reset the inactivity timer.
     * If currently inactive, fires onActivate and transitions to active first.
     */
    void poke();

    bool isActive() const { return active_.load(); }

private:
    static void timerCb(void* arg);
    void onTimeout();

    esp_timer_handle_t    timer_     {nullptr};
    uint64_t              timeoutUs_ {60'000'000ULL};   // default 60 s
    std::atomic<bool>     active_    {true};
    std::function<void()> onActivate_;
    std::function<void()> onDeactivate_;
};

} // namespace app
