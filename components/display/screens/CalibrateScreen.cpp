#include "CalibrateScreen.hpp"
#include "Theme.hpp"
#include "DisplayContext.hpp"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#include <cstdio>
#include <algorithm>

static const char* TAG     = "CalibrateScreen";
static const char* NVS_NS  = "water_cal";

namespace display {

using namespace theme;

// ── Construction ──────────────────────────────────────────────────────────────

CalibrateScreen::CalibrateScreen() {
    // Load persisted calibration values from NVS (best-effort)
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        uint32_t cap = 0;
        if (nvs_get_u32(h, "tank_litres", &cap) == ESP_OK && cap > 0) {
            capacityL_ = cap;
        }
        // NVS doesn't have a native float getter — store as uint32 bit-cast
        uint32_t bits = 0;
        if (nvs_get_u32(h, "cal_v_empty_b", &bits) == ESP_OK) {
            calVEmpty_ = *reinterpret_cast<float*>(&bits);
        }
        if (nvs_get_u32(h, "cal_v_full_b", &bits) == ESP_OK) {
            calVFull_ = *reinterpret_cast<float*>(&bits);
        }
        nvs_close(h);
    }
}

// ── Nav callback ──────────────────────────────────────────────────────────────

static void navCb(lv_event_t* e) {
    auto* ctx = static_cast<DisplayContext*>(lv_event_get_user_data(e));
    ctx->nextPage();
}

// ── create ────────────────────────────────────────────────────────────────────

void CalibrateScreen::create(DisplayContext* ctx) {
    screen_ = lv_obj_create(nullptr);
    lv_obj_set_size(screen_, LCD_W, LCD_H);
    lv_obj_set_style_bg_color(screen_, BG(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen_, 0, LV_PART_MAIN);

    // ── Header ────────────────────────────────────────────────────────────
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
    lv_label_set_text(title, "CALIBRATE");
    lv_obj_set_style_text_color(title, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    // ── Tank capacity row ─────────────────────────────────────────────────
    static constexpr int CAP_Y = HEADER_H + 6;
    lv_obj_t* capRow = lv_obj_create(screen_);
    lv_obj_set_size(capRow, LCD_W - 16, 36);
    lv_obj_set_pos(capRow, 8, CAP_Y);
    lv_obj_set_style_bg_color(capRow, SURFACE(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(capRow, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(capRow, BORDER(), LV_PART_MAIN);
    lv_obj_set_style_border_width(capRow, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(capRow, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(capRow, 0, LV_PART_MAIN);
    lv_obj_clear_flag(capRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* capKey = lv_label_create(capRow);
    lv_label_set_text(capKey, "Tank capacity");
    lv_obj_set_style_text_color(capKey, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(capKey, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(capKey, LV_ALIGN_LEFT_MID, 10, 0);

    // − button
    lv_obj_t* btnMinus = lv_obj_create(capRow);
    lv_obj_set_size(btnMinus, 26, 26);
    lv_obj_align(btnMinus, LV_ALIGN_RIGHT_MID, -62, 0);
    lv_obj_set_style_bg_color(btnMinus, CONTROL(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btnMinus, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(btnMinus, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btnMinus, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btnMinus, 0, LV_PART_MAIN);
    lv_obj_add_flag(btnMinus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btnMinus, onCapMinus, LV_EVENT_CLICKED, this);

    lv_obj_t* minusLbl = lv_label_create(btnMinus);
    lv_label_set_text(minusLbl, "-");
    lv_obj_set_style_text_color(minusLbl, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(minusLbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(minusLbl, LV_ALIGN_CENTER, 0, -1);

    // Capacity value label
    capLabel_ = lv_label_create(capRow);
    lv_obj_set_style_text_color(capLabel_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(capLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(capLabel_, LV_ALIGN_RIGHT_MID, -30, 0);
    refreshCapLabel();

    // + button
    lv_obj_t* btnPlus = lv_obj_create(capRow);
    lv_obj_set_size(btnPlus, 26, 26);
    lv_obj_align(btnPlus, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(btnPlus, CONTROL(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btnPlus, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(btnPlus, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btnPlus, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btnPlus, 0, LV_PART_MAIN);
    lv_obj_add_flag(btnPlus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btnPlus, onCapPlus, LV_EVENT_CLICKED, this);

    lv_obj_t* plusLbl = lv_label_create(btnPlus);
    lv_label_set_text(plusLbl, "+");
    lv_obj_set_style_text_color(plusLbl, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(plusLbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(plusLbl, LV_ALIGN_CENTER, 0, -1);

    // ── Mark Empty button ─────────────────────────────────────────────────
    static constexpr int EMPTY_Y = CAP_Y + 36 + 8;
    lv_obj_t* btnEmpty = lv_obj_create(screen_);
    lv_obj_set_size(btnEmpty, LCD_W - 16, 60);
    lv_obj_set_pos(btnEmpty, 8, EMPTY_Y);
    lv_obj_set_style_bg_color(btnEmpty, BLUE(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btnEmpty, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(btnEmpty, BLUE_BDR(), LV_PART_MAIN);
    lv_obj_set_style_border_width(btnEmpty, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(btnEmpty, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btnEmpty, 0, LV_PART_MAIN);
    lv_obj_clear_flag(btnEmpty, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btnEmpty, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btnEmpty, onMarkEmpty, LV_EVENT_CLICKED, this);

    lv_obj_t* emptyTitle = lv_label_create(btnEmpty);
    lv_label_set_text(emptyTitle, "Mark Empty");
    lv_obj_set_style_text_color(emptyTitle, lv_color_hex(0xe0f2fe), LV_PART_MAIN);
    lv_obj_set_style_text_font(emptyTitle, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(emptyTitle, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t* emptySub = lv_label_create(btnEmpty);
    lv_label_set_text(emptySub, "press when tank is empty");
    lv_obj_set_style_text_color(emptySub, lv_color_hex(0xbae6fd), LV_PART_MAIN);
    lv_obj_set_style_text_font(emptySub, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(emptySub, LV_ALIGN_CENTER, 0, 12);

    // ── Mark Full button ──────────────────────────────────────────────────
    static constexpr int FULL_Y = EMPTY_Y + 60 + 8;
    lv_obj_t* btnFull = lv_obj_create(screen_);
    lv_obj_set_size(btnFull, LCD_W - 16, 60);
    lv_obj_set_pos(btnFull, 8, FULL_Y);
    lv_obj_set_style_bg_color(btnFull, DARK_GRN(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btnFull, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(btnFull, GRN_BDR(), LV_PART_MAIN);
    lv_obj_set_style_border_width(btnFull, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(btnFull, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btnFull, 0, LV_PART_MAIN);
    lv_obj_clear_flag(btnFull, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btnFull, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btnFull, onMarkFull, LV_EVENT_CLICKED, this);

    lv_obj_t* fullTitle = lv_label_create(btnFull);
    lv_label_set_text(fullTitle, "Mark Full");
    lv_obj_set_style_text_color(fullTitle, lv_color_hex(0xdcfce7), LV_PART_MAIN);
    lv_obj_set_style_text_font(fullTitle, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(fullTitle, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t* fullSub = lv_label_create(btnFull);
    lv_label_set_text(fullSub, "press when tank is full");
    lv_obj_set_style_text_color(fullSub, lv_color_hex(0xbbf7d0), LV_PART_MAIN);
    lv_obj_set_style_text_font(fullSub, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(fullSub, LV_ALIGN_CENTER, 0, 12);

    // ── Raw readout panel ─────────────────────────────────────────────────
    static constexpr int RAW_Y = FULL_Y + 60 + 8;
    lv_obj_t* rawPanel = lv_obj_create(screen_);
    lv_obj_set_size(rawPanel, LCD_W - 16, 72);
    lv_obj_set_pos(rawPanel, 8, RAW_Y);
    lv_obj_set_style_bg_color(rawPanel, SURFACE(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rawPanel, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(rawPanel, BORDER(), LV_PART_MAIN);
    lv_obj_set_style_border_width(rawPanel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(rawPanel, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_left(rawPanel, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(rawPanel, 6, LV_PART_MAIN);
    lv_obj_clear_flag(rawPanel, LV_OBJ_FLAG_SCROLLABLE);

    // Helper lambda-like macro for raw rows
    auto makeRawRow = [&](lv_obj_t* parent, int yOff, const char* key, lv_obj_t** valOut) {
        lv_obj_t* keyLbl = lv_label_create(parent);
        lv_label_set_text(keyLbl, key);
        lv_obj_set_style_text_color(keyLbl, TEXT_MUT(), LV_PART_MAIN);
        lv_obj_set_style_text_font(keyLbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_pos(keyLbl, 0, yOff);
        lv_obj_set_width(keyLbl, 90);

        *valOut = lv_label_create(parent);
        lv_label_set_text(*valOut, "-");
        lv_obj_set_style_text_color(*valOut, TEXT_SEC(), LV_PART_MAIN);
        lv_obj_set_style_text_font(*valOut, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_align_to(*valOut, keyLbl, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    };

    makeRawRow(rawPanel,  0, "Raw ADC",        &rawAdc_);
    makeRawRow(rawPanel, 16, "Computed level", &computedM_);
    makeRawRow(rawPanel, 32, "Cal points",     &calPoints_);
    // rawVolts_ is unused as a separate label — folded into rawAdc_
    rawVolts_ = nullptr;

    // ── Nav button ────────────────────────────────────────────────────────
    lv_obj_t* nav = lv_obj_create(screen_);
    lv_obj_set_size(nav, LCD_W, NAV_H);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(nav, SURFACE(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nav, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(nav, BORDER(), LV_PART_MAIN);
    lv_obj_set_style_border_width(nav, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_radius(nav, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(nav, 0, LV_PART_MAIN);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(nav, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(nav, navCb, LV_EVENT_CLICKED, ctx);

    lv_obj_t* navLbl = lv_label_create(nav);
    lv_label_set_text(navLbl, "tap to cycle pages  >");
    lv_obj_set_style_text_color(navLbl, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_text_font(navLbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(navLbl, LV_ALIGN_CENTER, 0, 0);
}

// ── show ─────────────────────────────────────────────────────────────────────

void CalibrateScreen::show() {
    lv_screen_load(screen_);
}

// ── updateRaw ─────────────────────────────────────────────────────────────────

void CalibrateScreen::updateRaw(const WaterData& data) {
    lastVolts_ = data.rawVolts;

    char buf[48];

    snprintf(buf, sizeof(buf), "%u  (%.2f V)", data.rawAdc, data.rawVolts);
    lv_label_set_text(rawAdc_, buf);

    snprintf(buf, sizeof(buf), "%.2f m  \xe2\x86\x92  %.0f L", data.computedM, data.litres);
    lv_label_set_text(computedM_, buf);

    snprintf(buf, sizeof(buf), "%.2f V  \xc2\xb7  %.2f V", calVEmpty_, calVFull_);
    lv_label_set_text(calPoints_, buf);
}

// ── Button callbacks ──────────────────────────────────────────────────────────

void CalibrateScreen::onMarkEmpty(lv_event_t* e) {
    auto* self = static_cast<CalibrateScreen*>(lv_event_get_user_data(e));
    self->calVEmpty_ = self->lastVolts_;
    ESP_LOGI(TAG, "Calibration: empty = %.3f V", self->calVEmpty_);
    self->saveCalibration();
    // Refresh cal points display
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f V  \xc2\xb7  %.2f V", self->calVEmpty_, self->calVFull_);
    lv_label_set_text(self->calPoints_, buf);
}

void CalibrateScreen::onMarkFull(lv_event_t* e) {
    auto* self = static_cast<CalibrateScreen*>(lv_event_get_user_data(e));
    self->calVFull_ = self->lastVolts_;
    ESP_LOGI(TAG, "Calibration: full = %.3f V", self->calVFull_);
    self->saveCalibration();
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f V  \xc2\xb7  %.2f V", self->calVEmpty_, self->calVFull_);
    lv_label_set_text(self->calPoints_, buf);
}

void CalibrateScreen::onCapMinus(lv_event_t* e) {
    auto* self = static_cast<CalibrateScreen*>(lv_event_get_user_data(e));
    if (self->capacityL_ > 10) {
        self->capacityL_ -= 10;
        self->refreshCapLabel();
        self->saveCalibration();
    }
}

void CalibrateScreen::onCapPlus(lv_event_t* e) {
    auto* self = static_cast<CalibrateScreen*>(lv_event_get_user_data(e));
    self->capacityL_ += 10;
    self->refreshCapLabel();
    self->saveCalibration();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void CalibrateScreen::saveCalibration() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed");
        return;
    }
    nvs_set_u32(h, "tank_litres", capacityL_);

    // Store floats as bit-cast uint32
    uint32_t bits;
    bits = *reinterpret_cast<uint32_t*>(&calVEmpty_);
    nvs_set_u32(h, "cal_v_empty_b", bits);
    bits = *reinterpret_cast<uint32_t*>(&calVFull_);
    nvs_set_u32(h, "cal_v_full_b", bits);

    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Saved: cap=%luL empty=%.2fV full=%.2fV",
             (unsigned long)capacityL_, calVEmpty_, calVFull_);
}

void CalibrateScreen::refreshCapLabel() {
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu L", (unsigned long)capacityL_);
    lv_label_set_text(capLabel_, buf);
}

} // namespace display
