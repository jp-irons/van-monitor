#include "ActivityManager.hpp"

#include "logger/Logger.hpp"

static logger::Logger log{app::ActivityManager::TAG};

namespace app {

ActivityManager::ActivityManager() {
    log.debug("constructor");
}

ActivityManager::~ActivityManager() {
    if (timer_) {
        (void)esp_timer_stop(timer_);
        esp_timer_delete(timer_);
    }
}

void ActivityManager::start(uint32_t timeoutMs,
                             std::function<void()> onActivate,
                             std::function<void()> onDeactivate) {
    timeoutUs_    = static_cast<uint64_t>(timeoutMs) * 1000ULL;
    onActivate_   = std::move(onActivate);
    onDeactivate_ = std::move(onDeactivate);

    esp_timer_create_args_t args = {};
    args.callback = timerCb;
    args.arg      = this;
    args.name     = "activity_mgr";
    esp_err_t err = esp_timer_create(&args, &timer_);
    if (err != ESP_OK) {
        log.error("esp_timer_create failed: %s — activity manager disabled", esp_err_to_name(err));
        return;
    }

    // Start in active state; arm the initial timeout.
    active_ = true;
    err = esp_timer_start_once(timer_, timeoutUs_);
    if (err != ESP_OK) {
        log.error("esp_timer_start_once failed: %s", esp_err_to_name(err));
    }

    log.info("started — timeout %lu ms", (unsigned long)timeoutMs);
}

void ActivityManager::poke() {
	log.debug("poke");
    if (!timer_) return;

    // Stop before reading active_ to close the race with the timer callback.
    // esp_timer_stop returns ESP_ERR_INVALID_STATE if the timer has already
    // fired — that is expected and safe to ignore.
    (void)esp_timer_stop(timer_);

    if (!active_.exchange(true)) {
        // Transitioned Inactive → Active.
        log.info("activate");
        if (onActivate_) onActivate_();
    }

    esp_err_t err = esp_timer_start_once(timer_, timeoutUs_);
    if (err != ESP_OK) {
        log.error("esp_timer_start_once failed in poke: %s", esp_err_to_name(err));
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void ActivityManager::timerCb(void* arg) {
    static_cast<ActivityManager*>(arg)->onTimeout();
}

void ActivityManager::onTimeout() {
    if (active_.exchange(false)) {
        // Transitioned Active → Inactive.
        log.info("deactivate");
        if (onDeactivate_) onDeactivate_();
    }
}

} // namespace app
