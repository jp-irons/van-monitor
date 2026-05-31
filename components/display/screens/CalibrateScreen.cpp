#include "CalibrateScreen.hpp"
#include "Theme.hpp"
#include "DisplayContext.hpp"

#include "nvs_flash.h"
#include "nvs.h"
#include "logger/Logger.hpp"

#include <cstdio>
#include <algorithm>

static logger::Logger     log{display::CalibrateScreen::TAG};
static const char* NVS_NS = "water_cal";

namespace display {

using namespace theme;

// ── Layout constants ──────────────────────────────────────────────────────────

static constexpr int CAP_Y          = HEADER_H + 8;                            //  36 — capacity row top
static constexpr int WATER_DIV_Y    = CAP_Y + 36 + 8;                          //  80 — "WATER LEVEL" label
static constexpr int WATER_PANEL_Y  = WATER_DIV_Y + 14;                        //  94 — combined water panel top
static constexpr int WATER_PANEL_H  = 104;                                      //      (Empty row + Full row + voltage)
static constexpr int TILT_DIV_Y     = WATER_PANEL_Y + WATER_PANEL_H + 8;       // 206 — "TILT" label
static constexpr int FLAT_Y         = TILT_DIV_Y + 14;                         // 220 — Zero/Flat row top

// Derived: FLAT bottom = 220+40 = 260; NAV starts at 284 → 24 px gap

static constexpr int ROW_W          = LCD_W - 16;                              // 224 — row container width
static constexpr int ROW_H          = 40;
static constexpr int BTN_W          = 76;
static constexpr int BTN_H          = 28;

// ── Construction ──────────────────────────────────────────────────────────────

CalibrateScreen::CalibrateScreen() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        uint32_t cap = 0;
        if (nvs_get_u32(h, "tank_litres", &cap) == ESP_OK && cap > 0) {
            capacityL_ = cap;
        }
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

// ── setFlatCallback ───────────────────────────────────────────────────────────

void CalibrateScreen::setFlatCallback(std::function<void()> cb) {
    onFlatCb_ = std::move(cb);
}

// ── Nav callback ──────────────────────────────────────────────────────────────

static void navCb(lv_event_t* e) {
    auto* ctx = static_cast<DisplayContext*>(lv_event_get_user_data(e));
    ctx->nextPage();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

/** Build a section-divider label ("WATER LEVEL" / "TILT"). */
static lv_obj_t* makeDivLabel(lv_obj_t* parent, int y, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(lbl, 8, y);
    return lbl;
}

/**
 * Build a calibration-action row.
 *
 *  ┌──────────────────────────────────────────────────────┐
 *  │  <initValue>  (font 12, primary, left-mid)  ┌──────┐ │
 *  │                                             │ btn  │ │
 *  │                                             └──────┘ │
 *  └──────────────────────────────────────────────────────┘
 *
 * The value label pointer is written to *valueOut so the caller can update
 * it later via updateCalDisplay() / updateLevel().
 * Returns the button object.
 */
static lv_obj_t* makeCalRow(lv_obj_t* parent, int y,
                             const char* initValue,
                             lv_color_t btnBg, lv_color_t btnBdr,
                             const char* btnLabel,
                             lv_obj_t** valueOut) {
    // Row container
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, ROW_W, ROW_H);
    lv_obj_set_pos(row, 8, y);
    lv_obj_set_style_bg_color(row, SURFACE(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, BORDER(), LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Value label — left-aligned, vertically centred, constrained so it
    // cannot overflow into the button area.
    lv_obj_t* valLbl = lv_label_create(row);
    lv_label_set_text(valLbl, initValue);
    lv_obj_set_style_text_color(valLbl, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(valLbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_width(valLbl, ROW_W - BTN_W - 22);   // stay left of button
    lv_obj_align(valLbl, LV_ALIGN_LEFT_MID, 10, 0);
    *valueOut = valLbl;

    // Action button (right-aligned)
    lv_obj_t* btn = lv_obj_create(row);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(btn, btnBg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, btnBdr, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, btnLabel);
    lv_obj_set_style_text_color(lbl, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return btn;
}

// ── create ────────────────────────────────────────────────────────────────────

void CalibrateScreen::create(DisplayContext* ctx) {
    // Populate BtnCtx array before registering callbacks
    btnCtx_[0] = {this, 0};
    btnCtx_[1] = {this, 1};
    btnCtx_[2] = {this, 2};

    // ── Root screen ───────────────────────────────────────────────────────
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

    lv_obj_t* headerTitle = lv_label_create(header);
    lv_label_set_text(headerTitle, "CALIBRATE");
    lv_obj_set_style_text_color(headerTitle, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(headerTitle, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(headerTitle, LV_ALIGN_LEFT_MID, 10, 0);

    // ── Tank capacity row ─────────────────────────────────────────────────
    lv_obj_t* capRow = lv_obj_create(screen_);
    lv_obj_set_size(capRow, ROW_W, 36);
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
    lv_obj_set_style_text_font(capKey, &lv_font_montserrat_14, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(minusLbl, &lv_font_montserrat_14, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(plusLbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(plusLbl, LV_ALIGN_CENTER, 0, -1);

    // ── Section dividers — hint once per section, plain hyphen ───────────
    makeDivLabel(screen_, WATER_DIV_Y, "WATER LEVEL - hold 1.5s");
    makeDivLabel(screen_, TILT_DIV_Y,  "TILT - hold 1.5s");

    // ── Combined water panel ──────────────────────────────────────────────
    //
    // Internal layout (y positions within panel, pad_all=0):
    //   y=  5..38  Empty sub-row  (button + cal value)
    //   y= 38      1 px separator
    //   y= 39..76  Full sub-row   (button + cal value)
    //   y= 76      1 px separator
    //   y= 80..99  Voltage readout
    //   (WATER_PANEL_H = 104)
    {
        lv_obj_t* wp = lv_obj_create(screen_);
        lv_obj_set_size(wp, ROW_W, WATER_PANEL_H);
        lv_obj_set_pos(wp, 8, WATER_PANEL_Y);
        lv_obj_set_style_bg_color(wp, SURFACE(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(wp, OPA_FULL, LV_PART_MAIN);
        lv_obj_set_style_border_color(wp, BORDER(), LV_PART_MAIN);
        lv_obj_set_style_border_width(wp, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(wp, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(wp, 0, LV_PART_MAIN);
        lv_obj_clear_flag(wp, LV_OBJ_FLAG_SCROLLABLE);

        // ── Empty button (right) + value label (left) ─────────────────────
        btnEmpty_ = lv_obj_create(wp);
        lv_obj_set_size(btnEmpty_, BTN_W, BTN_H);
        lv_obj_set_pos(btnEmpty_, ROW_W - BTN_W - 6, 5);
        lv_obj_set_style_bg_color(btnEmpty_, BLUE(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btnEmpty_, OPA_FULL, LV_PART_MAIN);
        lv_obj_set_style_border_color(btnEmpty_, BLUE_BDR(), LV_PART_MAIN);
        lv_obj_set_style_border_width(btnEmpty_, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(btnEmpty_, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btnEmpty_, 0, LV_PART_MAIN);
        lv_obj_add_flag(btnEmpty_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(btnEmpty_, LV_OBJ_FLAG_SCROLLABLE);
        {
            lv_obj_t* lbl = lv_label_create(btnEmpty_);
            lv_label_set_text(lbl, "Empty");
            lv_obj_set_style_text_color(lbl, TEXT_PRI(), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
            lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        }

        emptyValLbl_ = lv_label_create(wp);
        lv_obj_set_style_text_color(emptyValLbl_, TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_text_font(emptyValLbl_, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_pos(emptyValLbl_, 10, 12);

        // ── Separator ─────────────────────────────────────────────────────
        lv_obj_t* sep1 = lv_obj_create(wp);
        lv_obj_set_size(sep1, ROW_W - 12, 1);
        lv_obj_set_pos(sep1, 6, 38);
        lv_obj_set_style_bg_color(sep1, BORDER(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sep1, OPA_FULL, LV_PART_MAIN);
        lv_obj_set_style_border_width(sep1, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(sep1, 0, LV_PART_MAIN);

        // ── Full button (right) + value label (left) ──────────────────────
        btnFull_ = lv_obj_create(wp);
        lv_obj_set_size(btnFull_, BTN_W, BTN_H);
        lv_obj_set_pos(btnFull_, ROW_W - BTN_W - 6, 44);
        lv_obj_set_style_bg_color(btnFull_, DARK_GRN(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btnFull_, OPA_FULL, LV_PART_MAIN);
        lv_obj_set_style_border_color(btnFull_, GRN_BDR(), LV_PART_MAIN);
        lv_obj_set_style_border_width(btnFull_, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(btnFull_, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btnFull_, 0, LV_PART_MAIN);
        lv_obj_add_flag(btnFull_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(btnFull_, LV_OBJ_FLAG_SCROLLABLE);
        {
            lv_obj_t* lbl = lv_label_create(btnFull_);
            lv_label_set_text(lbl, "Full");
            lv_obj_set_style_text_color(lbl, TEXT_PRI(), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
            lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        }

        fullValLbl_ = lv_label_create(wp);
        lv_obj_set_style_text_color(fullValLbl_, TEXT_PRI(), LV_PART_MAIN);
        lv_obj_set_style_text_font(fullValLbl_, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_pos(fullValLbl_, 10, 51);

        // ── Separator ─────────────────────────────────────────────────────
        lv_obj_t* sep2 = lv_obj_create(wp);
        lv_obj_set_size(sep2, ROW_W - 12, 1);
        lv_obj_set_pos(sep2, 6, 77);
        lv_obj_set_style_bg_color(sep2, BORDER(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sep2, OPA_FULL, LV_PART_MAIN);
        lv_obj_set_style_border_width(sep2, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(sep2, 0, LV_PART_MAIN);

        // ── Voltage readout ───────────────────────────────────────────────
        lv_obj_t* voltKey = lv_label_create(wp);
        lv_label_set_text(voltKey, "Voltage");
        lv_obj_set_style_text_color(voltKey, TEXT_MUT(), LV_PART_MAIN);
        lv_obj_set_style_text_font(voltKey, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_pos(voltKey, 10, 85);

        rawVolts_ = lv_label_create(wp);
        lv_label_set_text(rawVolts_, "-");
        lv_obj_set_style_text_color(rawVolts_, TEXT_SEC(), LV_PART_MAIN);
        lv_obj_set_style_text_font(rawVolts_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align_to(rawVolts_, voltKey, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    }

    // ── Zero (tilt) row ───────────────────────────────────────────────────
    btnFlat_ = makeCalRow(screen_, FLAT_Y,
                          "--", TEAL(), TEAL_BDR(), "Zero", &tiltLabel_);

    // Register long-press callbacks on all three buttons
    lv_obj_t* btns[3] = {btnEmpty_, btnFull_, btnFlat_};
    for (int i = 0; i < 3; ++i) {
        lv_obj_add_event_cb(btns[i], onBtnPressed,  LV_EVENT_PRESSED,    &btnCtx_[i]);
        lv_obj_add_event_cb(btns[i], onBtnReleased, LV_EVENT_RELEASED,   &btnCtx_[i]);
        lv_obj_add_event_cb(btns[i], onBtnReleased, LV_EVENT_PRESS_LOST, &btnCtx_[i]);
    }

    // Seed value labels from NVS-loaded calibration points
    updateCalDisplay();

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
    lv_label_set_text(navLbl, "Tap to cycle pages  >");
    lv_obj_set_style_text_color(navLbl, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(navLbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(navLbl, LV_ALIGN_CENTER, 0, 0);
}

// ── show ─────────────────────────────────────────────────────────────────────

void CalibrateScreen::show() {
    lv_screen_load(screen_);
}

// ── updateRaw ─────────────────────────────────────────────────────────────────

void CalibrateScreen::updateRaw(const WaterData& data) {
    lastVolts_ = data.rawVolts;

    if (dispVolts_ < 0.0f) {
        dispVolts_ = data.rawVolts;
    } else {
        dispVolts_ = DISP_ALPHA * dispVolts_ + (1.0f - DISP_ALPHA) * data.rawVolts;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%.2fV  (%.0fL)", dispVolts_, data.litres);
    lv_label_set_text(rawVolts_, buf);
}

// ── updateLevel ───────────────────────────────────────────────────────────────

void CalibrateScreen::updateLevel(const LevelData& data) {
    if (!tiltLabel_) return;

    // Button child label — first (and only) child of btnFlat_
    lv_obj_t* btnLbl = lv_obj_get_child(btnFlat_, 0);

    if (data.awaitingFlip) {
        lv_label_set_text(tiltLabel_, "Flip, then hold");
        if (btnLbl) lv_label_set_text(btnLbl, "Zero 2/2");
    } else {
        char buf[24];
        snprintf(buf, sizeof(buf), "X:%.1f\xc2\xb0  Y:%.1f\xc2\xb0", data.tiltX, data.tiltY);
        lv_label_set_text(tiltLabel_, buf);
        if (btnLbl) lv_label_set_text(btnLbl, "Zero");
    }
}

// ── Long-press callbacks ──────────────────────────────────────────────────────

void CalibrateScreen::onBtnPressed(lv_event_t* e) {
    auto* ctx  = static_cast<BtnCtx*>(lv_event_get_user_data(e));
    auto* self = ctx->self;
    self->pressStart_ = lv_tick_get();
    self->pressedId_  = ctx->id;

    // Dim slightly to indicate "armed; keep holding"
    lv_obj_t* btns[3] = {self->btnEmpty_, self->btnFull_, self->btnFlat_};
    lv_obj_set_style_bg_opa(btns[ctx->id], LV_OPA_60, LV_PART_MAIN);
}

void CalibrateScreen::onBtnReleased(lv_event_t* e) {
    auto* ctx  = static_cast<BtnCtx*>(lv_event_get_user_data(e));
    auto* self = ctx->self;

    // Restore opacity regardless of whether the hold was long enough
    lv_obj_t* btns[3] = {self->btnEmpty_, self->btnFull_, self->btnFlat_};
    lv_obj_set_style_bg_opa(btns[ctx->id], OPA_FULL, LV_PART_MAIN);

    if (self->pressedId_ != ctx->id) return;
    self->pressedId_ = -1;

    if (lv_tick_elaps(self->pressStart_) >= LONG_PRESS_MS) {
        self->handleLongPress(ctx->id);
    }
}

void CalibrateScreen::handleLongPress(int id) {
    switch (id) {
        case 0:  // Empty
            calVEmpty_ = lastVolts_;
            saveCalibration();
            updateCalDisplay();
            log.info("Calibration: empty = %.3f V", calVEmpty_);
            break;
        case 1:  // Full
            calVFull_ = lastVolts_;
            saveCalibration();
            updateCalDisplay();
            log.info("Calibration: full = %.3f V", calVFull_);
            break;
        case 2:  // Flat
            if (onFlatCb_) {
                log.info("Tilt flat calibration triggered");
                onFlatCb_();
            } else {
                log.warn("Flat pressed but no callback registered");
            }
            break;
    }
}

// ── Capacity callbacks ────────────────────────────────────────────────────────

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

void CalibrateScreen::updateCalDisplay() {
    char buf[16];
    if (emptyValLbl_) {
        snprintf(buf, sizeof(buf), "%.2fV", calVEmpty_);
        lv_label_set_text(emptyValLbl_, buf);
    }
    if (fullValLbl_) {
        snprintf(buf, sizeof(buf), "%.2fV", calVFull_);
        lv_label_set_text(fullValLbl_, buf);
    }
}

void CalibrateScreen::saveCalibration() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        log.warn("NVS open failed");
        return;
    }
    nvs_set_u32(h, "tank_litres", capacityL_);

    uint32_t bits;
    bits = *reinterpret_cast<uint32_t*>(&calVEmpty_);
    nvs_set_u32(h, "cal_v_empty_b", bits);
    bits = *reinterpret_cast<uint32_t*>(&calVFull_);
    nvs_set_u32(h, "cal_v_full_b", bits);

    nvs_commit(h);
    nvs_close(h);
    log.info("Saved: cap=%luL  empty=%.2fV  full=%.2fV",
             (unsigned long)capacityL_, calVEmpty_, calVFull_);
}

void CalibrateScreen::refreshCapLabel() {
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu L", (unsigned long)capacityL_);
    lv_label_set_text(capLabel_, buf);
}

} // namespace display
