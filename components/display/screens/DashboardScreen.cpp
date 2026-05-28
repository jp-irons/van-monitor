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

// Water section
static constexpr int WATER_Y    = HEADER_H;        // 28 — top of water section
static constexpr int WATER_BAR_H = 8;

// Level widget
static constexpr int LEVEL_Y        = 90;   // top of crosshair container
static constexpr int LEVEL_SIZE     = 96;   // crosshair container width & height
static constexpr int LEVEL_X        = (LCD_W - LEVEL_SIZE) / 2;  // centred = 72
static constexpr int LEVEL_LABEL_Y  = LEVEL_Y + LEVEL_SIZE + 4;  // 180
static constexpr int DOT_SIZE       = 10;
static constexpr int DOT_R          = DOT_SIZE / 2;               // 5
static constexpr float MAX_DISP_DEG = 5.f;  // tilt at which dot reaches canvas edge

// Battery section
static constexpr int DIV_Y   = 208;
static constexpr int BATT_Y  = DIV_Y + 8;  // 216

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

    // ── Water section ─────────────────────────────────────────────────────
    // Title
    lv_obj_t* waterTitle = lv_label_create(screen_);
    lv_label_set_text(waterTitle, "WATER");
    lv_obj_set_style_text_color(waterTitle, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(waterTitle, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_pos(waterTitle, 12, WATER_Y + 4);

    // Alarm background band — full-width, sits behind pct + litres labels.
    // Created before the labels so LVGL draws it underneath them.
    // Normally transparent; turns amber under alarm conditions.
    waterAlarmRow_ = lv_obj_create(screen_);
    lv_obj_set_size(waterAlarmRow_, LCD_W, 32);
    lv_obj_set_pos(waterAlarmRow_, 0, WATER_Y + 14);
    lv_obj_set_style_bg_opa(waterAlarmRow_, OPA_NONE, LV_PART_MAIN);
    lv_obj_set_style_border_width(waterAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(waterAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(waterAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(waterAlarmRow_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(waterAlarmRow_, LV_OBJ_FLAG_CLICKABLE);

    // Percentage label — left side of value row
    waterPct_ = lv_label_create(screen_);
    lv_label_set_text(waterPct_, "--%");
    lv_obj_set_style_text_color(waterPct_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(waterPct_, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_pos(waterPct_, 12, WATER_Y + 18);

    // Litres label — right-aligned on same row
    waterLitres_ = lv_label_create(screen_);
    lv_label_set_text(waterLitres_, "-- L");
    lv_obj_set_style_text_color(waterLitres_, CYAN(), LV_PART_MAIN);
    lv_obj_set_style_text_font(waterLitres_, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(waterLitres_, LV_ALIGN_TOP_RIGHT, -12, WATER_Y + 24);

    // Bar
    waterBar_ = lv_bar_create(screen_);
    lv_obj_set_size(waterBar_, LCD_W - 24, WATER_BAR_H);
    lv_obj_set_pos(waterBar_, 12, WATER_Y + 50);
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
    lv_obj_set_style_border_color(levelContainer_, BORDER(), LV_PART_MAIN);
    lv_obj_set_style_border_width(levelContainer_, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(levelContainer_, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(levelContainer_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(levelContainer_, LV_OBJ_FLAG_SCROLLABLE);

    // Horizontal crosshair line
    lv_obj_t* lineH = lv_obj_create(levelContainer_);
    lv_obj_set_size(lineH, LEVEL_SIZE, 1);
    lv_obj_set_style_bg_color(lineH, BORDER(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lineH, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(lineH, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(lineH, 0, LV_PART_MAIN);
    lv_obj_add_flag(lineH, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_pos(lineH, 0, LEVEL_SIZE / 2);

    // Vertical crosshair line
    lv_obj_t* lineV = lv_obj_create(levelContainer_);
    lv_obj_set_size(lineV, 1, LEVEL_SIZE);
    lv_obj_set_style_bg_color(lineV, BORDER(), LV_PART_MAIN);
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

    // Degree labels — below the container, left and right
    levelLabelX_ = lv_label_create(screen_);
    lv_label_set_text(levelLabelX_, "0.0\xc2\xb0");  // UTF-8 degree symbol
    lv_obj_set_style_text_color(levelLabelX_, GREEN(), LV_PART_MAIN);
    lv_obj_set_style_text_font(levelLabelX_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(levelLabelX_, LEVEL_X, LEVEL_LABEL_Y);

    levelLabelY_ = lv_label_create(screen_);
    lv_label_set_text(levelLabelY_, "0.0\xc2\xb0");
    lv_obj_set_style_text_color(levelLabelY_, GREEN(), LV_PART_MAIN);
    lv_obj_set_style_text_font(levelLabelY_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(levelLabelY_, LV_ALIGN_TOP_RIGHT, -(LCD_W - LEVEL_X - LEVEL_SIZE), LEVEL_LABEL_Y);

    // ── Divider ───────────────────────────────────────────────────────────
    lv_obj_t* div = lv_obj_create(screen_);
    lv_obj_set_size(div, LCD_W - 32, 1);
    lv_obj_set_pos(div, 16, DIV_Y);
    lv_obj_set_style_bg_color(div, BORDER(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(div, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(div, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(div, 0, LV_PART_MAIN);

    // ── Battery section ───────────────────────────────────────────────────
    // Alarm background band — behind SOC + voltage row.
    battAlarmRow_ = lv_obj_create(screen_);
    lv_obj_set_size(battAlarmRow_, LCD_W, 32);
    lv_obj_set_pos(battAlarmRow_, 0, BATT_Y - 2);
    lv_obj_set_style_bg_opa(battAlarmRow_, OPA_NONE, LV_PART_MAIN);
    lv_obj_set_style_border_width(battAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(battAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battAlarmRow_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(battAlarmRow_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(battAlarmRow_, LV_OBJ_FLAG_CLICKABLE);

    battSoc_ = lv_label_create(screen_);
    lv_label_set_text(battSoc_, "--%");
    lv_obj_set_style_text_color(battSoc_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battSoc_, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_pos(battSoc_, 36, BATT_Y);

    battVolts_ = lv_label_create(screen_);
    lv_label_set_text(battVolts_, "--.- V");
    lv_obj_set_style_text_color(battVolts_, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_text_font(battVolts_, &lv_font_montserrat_12, LV_PART_MAIN);
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

// ── updateBattery ─────────────────────────────────────────────────────────────

void DashboardScreen::updateBattery(const BatteryData& data) {
    char buf[64];

    lv_bar_set_value(battBar_, static_cast<int>(data.soc), LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%.0f%%", data.soc);
    lv_label_set_text(battSoc_, buf);

    snprintf(buf, sizeof(buf), "%.1f V  %.1f A", data.voltage, data.current);
    lv_label_set_text(battVolts_, buf);

    applyBatteryAlarm(data.soc);
}

// ── updateLevel ───────────────────────────────────────────────────────────────

void DashboardScreen::updateLevel(const LevelData& data) {
    // ── Dot position ──────────────────────────────────────────────────────
    static constexpr int HALF    = LEVEL_SIZE / 2;           // 48
    static constexpr int MAX_PX  = HALF - DOT_R;             // 43

    int dx = static_cast<int>(data.tiltX / MAX_DISP_DEG * MAX_PX);
    int dy = static_cast<int>(data.tiltY / MAX_DISP_DEG * MAX_PX);
    dx = std::clamp(dx, -MAX_PX, MAX_PX);
    dy = std::clamp(dy, -MAX_PX, MAX_PX);

    lv_obj_set_pos(levelDot_, HALF + dx - DOT_R, HALF + dy - DOT_R);

    // ── Colour based on worst-axis severity ───────────────────────────────
    float worst = std::max(fabsf(data.tiltX), fabsf(data.tiltY));
    lv_color_t col;
    if (worst >= LEVEL_ERR_DEG)       col = RED();
    else if (worst >= LEVEL_WARN_DEG) col = AMBER();
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
        lv_obj_set_style_text_color(battVolts_, TEXT_MUT(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(battBar_, GREEN(), LV_PART_INDICATOR);
    } else {
        // Normal
        lv_obj_set_style_bg_opa(battAlarmRow_, OPA_NONE, LV_PART_MAIN);
        lv_obj_set_style_text_color(battSoc_,   TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_text_color(battVolts_, TEXT_MUT(), LV_PART_MAIN);
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
    lv_label_set_text(lbl, "tap to cycle pages  >");
    lv_obj_set_style_text_color(lbl, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return btn;
}

} // namespace display
