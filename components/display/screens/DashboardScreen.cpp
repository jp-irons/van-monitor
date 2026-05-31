#include "DashboardScreen.hpp"
#include "Theme.hpp"
#include "DisplayContext.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace display {

using namespace theme;

// ── Layout constants ──────────────────────────────────────────────────────────

// Water section — starts at top of screen (no header bar)
static constexpr int WATER_Y    = 0;
static constexpr int WATER_BAR_H = 8;

// Level widget
static constexpr int LEVEL_Y        = 68;   // top of crosshair container
static constexpr int LEVEL_SIZE     = 120;  // crosshair container width & height
static constexpr int LEVEL_X        = 90;   // nudged right to leave room for Y-axis degree label
static constexpr int LEVEL_LABEL_Y  = LEVEL_Y + LEVEL_SIZE + 8;  // 196 — row shared by BATTERY label + X-axis degree
static constexpr int DOT_SIZE       = 12;
static constexpr int DOT_R          = DOT_SIZE / 2;               // 6
static constexpr float MAX_DISP_DEG = 7.f;  // tilt at which dot reaches canvas edge

// Battery section
static constexpr int BATT_Y  = 214;

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

    // ── Status dots — top-right, vertically centred on the WATER label row ──
    // (No header bar — dots share the WATER title row)
    dotWifi_ = lv_obj_create(screen_);
    lv_obj_set_size(dotWifi_, 6, 6);
    lv_obj_set_style_radius(dotWifi_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dotWifi_, GREEN(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dotWifi_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(dotWifi_, 0, LV_PART_MAIN);
    lv_obj_align(dotWifi_, LV_ALIGN_TOP_RIGHT, -22, 8);

    dotMqtt_ = lv_obj_create(screen_);
    lv_obj_set_size(dotMqtt_, 6, 6);
    lv_obj_set_style_radius(dotMqtt_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dotMqtt_, CYAN(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dotMqtt_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(dotMqtt_, 0, LV_PART_MAIN);
    lv_obj_align(dotMqtt_, LV_ALIGN_TOP_RIGHT, -10, 8);

    // ── Water section ─────────────────────────────────────────────────────
    // Title
    lv_obj_t* waterTitle = lv_label_create(screen_);
    lv_label_set_text(waterTitle, "WATER");
    lv_obj_set_style_text_color(waterTitle, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(waterTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(waterTitle, 12, WATER_Y + 4);

    // Alarm background band — full-width, sits behind pct + litres labels.
    // Created before the labels so LVGL draws it underneath them.
    // Normally transparent; turns amber under alarm conditions.
    waterAlarmRow_ = lv_obj_create(screen_);
    lv_obj_set_size(waterAlarmRow_, LCD_W, 32);
    lv_obj_set_pos(waterAlarmRow_, 0, WATER_Y + 20);
    lv_obj_set_style_bg_opa(waterAlarmRow_, OPA_NONE, LV_PART_MAIN);
    lv_obj_set_style_border_width(waterAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(waterAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(waterAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(waterAlarmRow_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(waterAlarmRow_, LV_OBJ_FLAG_CLICKABLE);

    // Percentage label — left side of value row (extra gap below title)
    waterPct_ = lv_label_create(screen_);
    lv_label_set_text(waterPct_, "--%");
    lv_obj_set_style_text_color(waterPct_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(waterPct_, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_pos(waterPct_, 12, WATER_Y + 24);

    // Litres label — right-aligned on same row
    waterLitres_ = lv_label_create(screen_);
    lv_label_set_text(waterLitres_, "-- L");
    lv_obj_set_style_text_color(waterLitres_, CYAN(), LV_PART_MAIN);
    lv_obj_set_style_text_font(waterLitres_, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(waterLitres_, LV_ALIGN_TOP_RIGHT, -12, WATER_Y + 30);

    // Bar
    waterBar_ = lv_bar_create(screen_);
    lv_obj_set_size(waterBar_, LCD_W - 24, WATER_BAR_H);
    lv_obj_set_pos(waterBar_, 12, WATER_Y + 54);
    lv_bar_set_range(waterBar_, 0, 100);
    lv_bar_set_value(waterBar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(waterBar_, TRACK(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(waterBar_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(waterBar_, CYAN(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(waterBar_, OPA_FULL, LV_PART_INDICATOR);
    lv_obj_set_style_radius(waterBar_, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(waterBar_, 4, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(waterBar_, 0, LV_PART_MAIN);

    // ── Level widget ──────────────────────────────────────────────────────
    // Container — dark background, fixed position, no scroll, no layout
    levelContainer_ = lv_obj_create(screen_);
    lv_obj_set_size(levelContainer_, LEVEL_SIZE, LEVEL_SIZE);
    lv_obj_set_pos(levelContainer_, LEVEL_X, LEVEL_Y);
    lv_obj_set_style_bg_color(levelContainer_, SURFACE(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(levelContainer_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(levelContainer_, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_border_width(levelContainer_, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(levelContainer_, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(levelContainer_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(levelContainer_, LV_OBJ_FLAG_SCROLLABLE);

    // Horizontal crosshair line
    lv_obj_t* lineH = lv_obj_create(levelContainer_);
    lv_obj_set_size(lineH, LEVEL_SIZE, 1);
    lv_obj_set_style_bg_color(lineH, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lineH, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(lineH, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(lineH, 0, LV_PART_MAIN);
    lv_obj_add_flag(lineH, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_pos(lineH, 0, LEVEL_SIZE / 2);

    // Vertical crosshair line
    lv_obj_t* lineV = lv_obj_create(levelContainer_);
    lv_obj_set_size(lineV, 1, LEVEL_SIZE);
    lv_obj_set_style_bg_color(lineV, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lineV, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(lineV, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(lineV, 0, LV_PART_MAIN);
    lv_obj_add_flag(lineV, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_pos(lineV, LEVEL_SIZE / 2, 0);

    // Dot — circular, starts at centre
    levelDot_ = lv_obj_create(levelContainer_);
    lv_obj_set_size(levelDot_, DOT_SIZE, DOT_SIZE);
    lv_obj_set_style_radius(levelDot_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(levelDot_, GREEN(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(levelDot_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(levelDot_, 0, LV_PART_MAIN);
    lv_obj_add_flag(levelDot_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_pos(levelDot_, LEVEL_SIZE / 2 - DOT_R, LEVEL_SIZE / 2 - DOT_R);

    // X-axis degree label — below container, centred on widget midpoint.
    // Shares the BATTERY label row (LEVEL_LABEL_Y).
    levelLabelX_ = lv_label_create(screen_);
    lv_label_set_text(levelLabelX_, "0.0\xc2\xb0");  // UTF-8 degree symbol
    lv_obj_set_style_text_color(levelLabelX_, GREEN(), LV_PART_MAIN);
    lv_obj_set_style_text_font(levelLabelX_, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_pos(levelLabelX_, LEVEL_X, LEVEL_LABEL_Y);
    lv_obj_set_width(levelLabelX_, LEVEL_SIZE);
    lv_obj_set_style_text_align(levelLabelX_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // Y-axis degree label — left of container, vertically centred with widget.
    // Right-aligned within the left column so it sits close to the widget edge.
    levelLabelY_ = lv_label_create(screen_);
    lv_label_set_text(levelLabelY_, "0.0\xc2\xb0");
    lv_obj_set_style_text_color(levelLabelY_, GREEN(), LV_PART_MAIN);
    lv_obj_set_style_text_font(levelLabelY_, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_pos(levelLabelY_, 12, LEVEL_Y + LEVEL_SIZE / 2 - 8);
    lv_obj_set_width(levelLabelY_, LEVEL_X - 16);
    lv_obj_set_style_text_align(levelLabelY_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    // ── Battery section ───────────────────────────────────────────────────
    // Alarm background band — behind SOC + voltage row.
    battAlarmRow_ = lv_obj_create(screen_);
    lv_obj_set_size(battAlarmRow_, LCD_W, 34);
    lv_obj_set_pos(battAlarmRow_, 0, BATT_Y - 2);
    lv_obj_set_style_bg_opa(battAlarmRow_, OPA_NONE, LV_PART_MAIN);
    lv_obj_set_style_border_width(battAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(battAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(battAlarmRow_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(battAlarmRow_, LV_OBJ_FLAG_CLICKABLE);

    // Section title — shares the degree-label row below the level widget.
    // Created after alarm band so it z-orders above it.
    battTitle_ = lv_label_create(screen_);
    lv_label_set_text(battTitle_, "BATTERY");
    lv_obj_set_style_text_color(battTitle_, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battTitle_, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(battTitle_, 12, LEVEL_LABEL_Y);

    battSoc_ = lv_label_create(screen_);
    lv_label_set_text(battSoc_, "--%");
    lv_obj_set_style_text_color(battSoc_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battSoc_, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_pos(battSoc_, 12, BATT_Y);

    battVolts_ = lv_label_create(screen_);
    lv_label_set_text(battVolts_, "--.- V");
    lv_obj_set_style_text_color(battVolts_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battVolts_, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(battVolts_, LV_ALIGN_TOP_RIGHT, -12, BATT_Y + 5);

    battBar_ = lv_bar_create(screen_);
    lv_obj_set_size(battBar_, LCD_W - 24, 9);
    lv_obj_set_pos(battBar_, 12, BATT_Y + 34);
    lv_bar_set_range(battBar_, 0, 100);
    lv_bar_set_value(battBar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(battBar_, TRACK(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(battBar_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(battBar_, GREEN(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(battBar_, OPA_FULL, LV_PART_INDICATOR);
    lv_obj_set_style_radius(battBar_, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(battBar_, 4, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(battBar_, 0, LV_PART_MAIN);

    // Solar / load / net stats row — below the bar
    battSolar_ = lv_label_create(screen_);
    lv_label_set_text(battSolar_, "");
    lv_obj_set_style_text_color(battSolar_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battSolar_, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(battSolar_, 12, BATT_Y + 46);
    lv_obj_set_width(battSolar_, LCD_W - 24);
    lv_label_set_long_mode(battSolar_, LV_LABEL_LONG_CLIP);

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

    lv_bar_set_value(waterBar_, static_cast<int>(data.pct), LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%.0f%%", data.pct);
    lv_label_set_text(waterPct_, buf);

    snprintf(buf, sizeof(buf), "%.0f L", data.litres);
    lv_label_set_text(waterLitres_, buf);

    applyWaterAlarm(data.pct);
}

// ── fmtKwh ────────────────────────────────────────────────────────────────────

static void fmtKwh(char* buf, size_t len, float kwh) {
    if      (kwh < 10.f)  snprintf(buf, len, "%.2f kWh", kwh);
    else if (kwh < 100.f) snprintf(buf, len, "%.1f kWh", kwh);
    else                  snprintf(buf, len, "%.0f kWh", kwh);
}

// ── updateBattery ─────────────────────────────────────────────────────────────

void DashboardScreen::updateBattery(const BatteryData& data) {
    char buf[64];

    lv_bar_set_value(battBar_, static_cast<int>(data.soc), LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%.0f%%", data.soc);
    lv_label_set_text(battSoc_, buf);

    snprintf(buf, sizeof(buf), "%.1f V  %.1f A", data.voltage, data.current);
    lv_label_set_text(battVolts_, buf);

    char kwhBuf[16];
    fmtKwh(kwhBuf, sizeof(kwhBuf), data.solarYieldKwh);
    snprintf(buf, sizeof(buf), "Solar %.0fW  %s today", data.solarW, kwhBuf);
    lv_label_set_text(battSolar_, buf);

    applyBatteryAlarm(data.soc);
}

// ── updateLevel ───────────────────────────────────────────────────────────────

void DashboardScreen::updateLevel(const LevelData& data) {
    // ── Dot position ──────────────────────────────────────────────────────
    static constexpr int HALF    = LEVEL_SIZE / 2;           // 48
    static constexpr int MAX_PX  = HALF - DOT_R;             // 43

    // Log-compressed mapping: linear inside ±KNEE_DEG, log-tapered beyond.
    // Gives fine sensitivity near level where it matters for parking.
    static constexpr float KNEE_DEG  = 3.0f;
    static constexpr float LOG_SCALE =
        (MAX_DISP_DEG - KNEE_DEG) / logf(1.f + MAX_DISP_DEG - KNEE_DEG);

    auto compress = [&](float raw) -> int {
        float a = fabsf(raw);
        float d = (a <= KNEE_DEG)
            ? a
            : KNEE_DEG + logf(1.f + (a - KNEE_DEG)) * LOG_SCALE;
        float px = (raw >= 0 ? 1.f : -1.f) * d / MAX_DISP_DEG * MAX_PX;
        return std::clamp(static_cast<int>(px), -MAX_PX, MAX_PX);
    };

    int dx = compress(data.tiltX);
    int dy = compress(data.tiltY);

    lv_obj_set_pos(levelDot_, HALF + dx - DOT_R, HALF + dy - DOT_R);

    // ── Colour based on worst-axis severity ───────────────────────────────
    float worst = std::max(fabsf(data.tiltX), fabsf(data.tiltY));
    lv_color_t col;
    if (worst >= LEVEL_ERR_DEG)       col = LEVEL_ERR();
    else if (worst >= LEVEL_WARN_DEG) col = LEVEL_WARN();
    else                               col = GREEN();

    lv_obj_set_style_bg_color(levelDot_, col, LV_PART_MAIN);

    // ── Degree labels — only redraw when formatted text changes (anti-jitter)
    // Degree symbol: UTF-8 0xC2 0xB0
    char bufX[10], bufY[10];
    snprintf(bufX, sizeof(bufX), "%.1f\xc2\xb0", data.tiltX);
    snprintf(bufY, sizeof(bufY), "%.1f\xc2\xb0", data.tiltY);

    if (strcmp(bufX, prevLabelX_) != 0) {
        lv_label_set_text(levelLabelX_, bufX);
        lv_obj_set_style_text_color(levelLabelX_, col, LV_PART_MAIN);
        strncpy(prevLabelX_, bufX, sizeof(prevLabelX_) - 1);
    }
    if (strcmp(bufY, prevLabelY_) != 0) {
        lv_label_set_text(levelLabelY_, bufY);
        lv_obj_set_style_text_color(levelLabelY_, col, LV_PART_MAIN);
        strncpy(prevLabelY_, bufY, sizeof(prevLabelY_) - 1);
    }
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
    if (pct <= ALARM_LOW_PCT || pct >= ALARM_HIGH_PCT) {
        // Alarm — amber band behind value row, black text, amber bar
        lv_obj_set_style_bg_color(waterAlarmRow_, AMBER(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(waterAlarmRow_, OPA_FULL, LV_PART_MAIN);
        lv_obj_set_style_text_color(waterPct_,    BLACK(), LV_PART_MAIN);
        lv_obj_set_style_text_color(waterLitres_, BLACK(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(waterBar_, AMBER(), LV_PART_INDICATOR);
    } else {
        // Normal — transparent band, default text colours, cyan bar
        lv_obj_set_style_bg_opa(waterAlarmRow_, OPA_NONE, LV_PART_MAIN);
        lv_obj_set_style_text_color(waterPct_,    TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_text_color(waterLitres_, CYAN(),     LV_PART_MAIN);
        lv_obj_set_style_bg_color(waterBar_, CYAN(), LV_PART_INDICATOR);
    }
}

void DashboardScreen::applyBatteryAlarm(float soc) {
    if (soc <= ALARM_LOW_PCT) {
        // Critical — red band behind SOC row, black text, red bar
        lv_obj_set_style_bg_color(battAlarmRow_, RED(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(battAlarmRow_, OPA_FULL, LV_PART_MAIN);
        lv_obj_set_style_text_color(battSoc_,   BLACK(), LV_PART_MAIN);
        lv_obj_set_style_text_color(battVolts_, BLACK(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(battBar_, RED(), LV_PART_INDICATOR);
    } else if (soc >= ALARM_HIGH_PCT) {
        // Nearly charged — positive state, no alarm band
        lv_obj_set_style_bg_opa(battAlarmRow_, OPA_NONE, LV_PART_MAIN);
        lv_obj_set_style_text_color(battSoc_,   TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_text_color(battVolts_, TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(battBar_, GREEN(), LV_PART_INDICATOR);
    } else {
        // Normal
        lv_obj_set_style_bg_opa(battAlarmRow_, OPA_NONE, LV_PART_MAIN);
        lv_obj_set_style_text_color(battSoc_,   TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_text_color(battVolts_, TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(battBar_, GREEN(), LV_PART_INDICATOR);
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
    lv_label_set_text(lbl, "Tap to cycle pages  >");
    lv_obj_set_style_text_color(lbl, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return btn;
}

} // namespace display
