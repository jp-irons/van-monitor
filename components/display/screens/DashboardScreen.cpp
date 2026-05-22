#include "DashboardScreen.hpp"
#include "Theme.hpp"
#include "DisplayContext.hpp"

#include <cstdio>
#include <cmath>

namespace display {

using namespace theme;

// ── Nav button callback ───────────────────────────────────────────────────────

static void navCb(lv_event_t* e) {
    auto* ctx = static_cast<DisplayContext*>(lv_event_get_user_data(e));
    ctx->nextPage();
}

// ── create ────────────────────────────────────────────────────────────────────

void DashboardScreen::create(DisplayContext* ctx) {
    // ── Root screen ───────────────────────────────────────────────────────
    screen_ = lv_obj_create(nullptr);
    lv_obj_set_size(screen_, LCD_W, LCD_H);
    lv_obj_set_style_bg_color(screen_, BG(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen_, 0, LV_PART_MAIN);

    // ── Header bar ────────────────────────────────────────────────────────
    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_size(header, LCD_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, SURFACE(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "VAN MONITOR");
    lv_obj_set_style_text_color(title, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    // Status dots (Wi-Fi and MQTT) — 6×6 circles, right-aligned
    dotWifi_ = lv_obj_create(header);
    lv_obj_set_size(dotWifi_, 6, 6);
    lv_obj_set_style_radius(dotWifi_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dotWifi_, GREEN(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dotWifi_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(dotWifi_, 0, LV_PART_MAIN);
    lv_obj_align(dotWifi_, LV_ALIGN_RIGHT_MID, -22, 0);

    dotMqtt_ = lv_obj_create(header);
    lv_obj_set_size(dotMqtt_, 6, 6);
    lv_obj_set_style_radius(dotMqtt_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dotMqtt_, CYAN(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dotMqtt_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(dotMqtt_, 0, LV_PART_MAIN);
    lv_obj_align(dotMqtt_, LV_ALIGN_RIGHT_MID, -10, 0);

    // ── Warning banner (hidden initially) ────────────────────────────────
    banner_ = lv_obj_create(screen_);
    lv_obj_set_size(banner_, LCD_W, BANNER_H);
    lv_obj_set_pos(banner_, 0, HEADER_H);
    lv_obj_set_style_bg_color(banner_, lv_color_hex(0x451a03), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(banner_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(banner_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(banner_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(banner_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(banner_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(banner_, LV_OBJ_FLAG_HIDDEN);  // hidden by default

    bannerLabel_ = lv_label_create(banner_);
    lv_label_set_text(bannerLabel_, "");
    lv_obj_set_style_text_color(bannerLabel_, AMBER(), LV_PART_MAIN);
    lv_obj_set_style_text_font(bannerLabel_, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(bannerLabel_, LV_ALIGN_CENTER, 0, 0);

    // ── Water arc gauge ───────────────────────────────────────────────────
    // The arc is centred in the water section (y 28–189, height 162)
    // Water section spans y=48 (below banner) to y=190 (divider) = 142 px.
    // Arc widget at radius 48 is 120 px tall → 11 px margin top & bottom → centre y=119.
    static constexpr int ARC_RADIUS   = 48;
    static constexpr int ARC_WIDTH    = 10;
    static constexpr int ARC_SIZE     = ARC_RADIUS * 2 + ARC_WIDTH * 2 + 4;  // 120
    static constexpr int ARC_Y_TOP    = HEADER_H + BANNER_H + 11;            // 59
    static constexpr int ARC_Y_CENTER = ARC_Y_TOP + ARC_SIZE / 2;            // 119

    arc_ = lv_arc_create(screen_);
    lv_obj_set_size(arc_, ARC_SIZE, ARC_SIZE);
    lv_obj_align(arc_, LV_ALIGN_TOP_MID, 0, ARC_Y_TOP);

    // Arc spans 135° → 405° (270° sweep, gap at bottom)
    lv_arc_set_bg_angles(arc_, 135, 45);
    lv_arc_set_angles(arc_, 135, 45);
    lv_arc_set_range(arc_, 0, 100);
    lv_arc_set_value(arc_, 0);
    lv_arc_set_mode(arc_, LV_ARC_MODE_NORMAL);

    // Remove the knob and click interaction
    lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_color(arc_, TRACK(), LV_PART_MAIN);      // background track
    lv_obj_set_style_arc_width(arc_, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_, CYAN(), LV_PART_INDICATOR);  // value indicator
    lv_obj_set_style_arc_width(arc_, ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc_, OPA_NONE, LV_PART_MAIN);
    lv_obj_set_style_border_width(arc_, 0, LV_PART_MAIN);

    // Percentage label — large, centred over arc
    arcPct_ = lv_label_create(screen_);
    lv_label_set_text(arcPct_, "--%");
    lv_obj_set_style_text_color(arcPct_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(arcPct_, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_align(arcPct_, LV_ALIGN_TOP_MID, 0, ARC_Y_CENTER - 22);

    // Litres label — below percentage
    arcLitres_ = lv_label_create(screen_);
    lv_label_set_text(arcLitres_, "-- L");
    lv_obj_set_style_text_color(arcLitres_, CYAN(), LV_PART_MAIN);
    lv_obj_set_style_text_font(arcLitres_, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(arcLitres_, LV_ALIGN_TOP_MID, 0, ARC_Y_CENTER + 8);

    // "water" unit hint
    lv_obj_t* waterUnit = lv_label_create(screen_);
    lv_label_set_text(waterUnit, "water");
    lv_obj_set_style_text_color(waterUnit, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_text_font(waterUnit, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(waterUnit, LV_ALIGN_TOP_MID, 0, ARC_Y_CENTER + 26);

    // ── Divider ───────────────────────────────────────────────────────────
    static constexpr int DIV_Y = 190;
    lv_obj_t* div = lv_obj_create(screen_);
    lv_obj_set_size(div, LCD_W - 32, 1);
    lv_obj_set_pos(div, 16, DIV_Y);
    lv_obj_set_style_bg_color(div, BORDER(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(div, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(div, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(div, 0, LV_PART_MAIN);

    // ── Battery section ───────────────────────────────────────────────────
    static constexpr int BATT_Y = DIV_Y + 8;  // 198

    // SOC percentage — large text
    battSoc_ = lv_label_create(screen_);
    lv_label_set_text(battSoc_, "--%");
    lv_obj_set_style_text_color(battSoc_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battSoc_, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_pos(battSoc_, 36, BATT_Y);

    // Voltage / current — right-aligned on same row
    battVolts_ = lv_label_create(screen_);
    lv_label_set_text(battVolts_, "--.- V");
    lv_obj_set_style_text_color(battVolts_, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battVolts_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(battVolts_, LV_ALIGN_TOP_RIGHT, -12, BATT_Y + 5);

    // Battery bar
    battBar_ = lv_bar_create(screen_);
    lv_obj_set_size(battBar_, LCD_W - 24, 9);
    lv_obj_set_pos(battBar_, 12, BATT_Y + 30);
    lv_bar_set_range(battBar_, 0, 100);
    lv_bar_set_value(battBar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(battBar_, TRACK(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(battBar_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(battBar_, GREEN(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(battBar_, OPA_FULL, LV_PART_INDICATOR);
    lv_obj_set_style_radius(battBar_, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(battBar_, 4, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(battBar_, 0, LV_PART_MAIN);

    // Status / stats row (solar, load, net) or alarm text
    battStatus_ = lv_label_create(screen_);
    lv_label_set_text(battStatus_, "");
    lv_obj_set_style_text_color(battStatus_, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battStatus_, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_pos(battStatus_, 12, BATT_Y + 46);
    lv_obj_set_width(battStatus_, LCD_W - 24);
    lv_label_set_long_mode(battStatus_, LV_LABEL_LONG_CLIP);

    // ── Nav button ────────────────────────────────────────────────────────
    makeNavButton(screen_, ctx);
}

// ── show ─────────────────────────────────────────────────────────────────────

void DashboardScreen::show() {
    lv_screen_load(screen_);
}

// ── updateWater ───────────────────────────────────────────────────────────────

void DashboardScreen::updateWater(const WaterData& data) {
    char buf[32];

    lv_arc_set_value(arc_, static_cast<int>(data.pct));

    snprintf(buf, sizeof(buf), "%.0f%%", data.pct);
    lv_label_set_text(arcPct_, buf);

    snprintf(buf, sizeof(buf), "%.0f L", data.litres);
    lv_label_set_text(arcLitres_, buf);

    applyWaterAlarm(data.pct);
}

// ── updateBattery ─────────────────────────────────────────────────────────────

void DashboardScreen::updateBattery(const BatteryData& data) {
    char buf[64];

    lv_bar_set_value(battBar_, static_cast<int>(data.soc), LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%.0f%%", data.soc);
    lv_label_set_text(battSoc_, buf);

    snprintf(buf, sizeof(buf), "%.1f V  %.1f A", data.voltage, data.current);
    lv_label_set_text(battVolts_, buf);

    float net = data.solarW - data.loadW;
    snprintf(buf, sizeof(buf), "Solar %.0fW   Load %.0fW   Net %+.0fW",
             data.solarW, data.loadW, net);
    lv_label_set_text(battStatus_, buf);

    applyBatteryAlarm(data.soc);
}

// ── setWifiOk / setMqttOk ─────────────────────────────────────────────────────

void DashboardScreen::setWifiOk(bool ok) {
    lv_obj_set_style_bg_color(dotWifi_, ok ? GREEN() : RED(), LV_PART_MAIN);
}

void DashboardScreen::setMqttOk(bool ok) {
    lv_obj_set_style_bg_color(dotMqtt_, ok ? CYAN() : RED(), LV_PART_MAIN);
}

// ── Private: alarm state ──────────────────────────────────────────────────────

void DashboardScreen::applyWaterAlarm(float pct) {
    if (pct <= ALARM_LOW_PCT) {
        // Near empty — amber arc + text + banner
        lv_obj_set_style_arc_color(arc_, AMBER(), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(arcPct_,    AMBER(), LV_PART_MAIN);
        lv_obj_set_style_text_color(arcLitres_, AMBER(), LV_PART_MAIN);
        lv_label_set_text(bannerLabel_, "WATER LOW");
        lv_obj_clear_flag(banner_, LV_OBJ_FLAG_HIDDEN);
    } else if (pct >= ALARM_HIGH_PCT) {
        // Near full — amber arc + text + banner
        lv_obj_set_style_arc_color(arc_, AMBER(), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(arcPct_,    AMBER(), LV_PART_MAIN);
        lv_obj_set_style_text_color(arcLitres_, AMBER(), LV_PART_MAIN);
        lv_label_set_text(bannerLabel_, "TANK NEARLY FULL");
        lv_obj_clear_flag(banner_, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Normal — cyan arc
        lv_obj_set_style_arc_color(arc_, CYAN(), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(arcPct_,    TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_text_color(arcLitres_, CYAN(), LV_PART_MAIN);
        lv_obj_add_flag(banner_, LV_OBJ_FLAG_HIDDEN);
    }
}

void DashboardScreen::applyBatteryAlarm(float soc) {
    char buf[48];

    if (soc <= ALARM_LOW_PCT) {
        // Critical — red bar + red SOC + alarm text
        lv_obj_set_style_bg_color(battBar_, RED(), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(battSoc_, RED(), LV_PART_MAIN);
        snprintf(buf, sizeof(buf), "Battery critical");
        lv_obj_set_style_text_color(battStatus_, RED(), LV_PART_MAIN);
        lv_label_set_text(battStatus_, buf);
    } else if (soc >= ALARM_HIGH_PCT) {
        // Nearly charged — keep green, positive label
        lv_obj_set_style_bg_color(battBar_, GREEN(), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(battSoc_, TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_text_color(battStatus_, GREEN(), LV_PART_MAIN);
        lv_label_set_text(battStatus_, "Nearly charged");
    } else {
        // Normal — green bar, muted stats
        lv_obj_set_style_bg_color(battBar_, GREEN(), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(battSoc_, TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_text_color(battStatus_, TEXT_MUT(), LV_PART_MAIN);
        // stats text is set in updateBattery() — no override needed here
    }
}

// ── Private: nav button ───────────────────────────────────────────────────────

lv_obj_t* DashboardScreen::makeNavButton(lv_obj_t* parent, DisplayContext* ctx) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, LCD_W, NAV_H);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn, SURFACE(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, BORDER(), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, navCb, LV_EVENT_CLICKED, ctx);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "tap to cycle pages  >");
    lv_obj_set_style_text_color(lbl, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return btn;
}

} // namespace display
