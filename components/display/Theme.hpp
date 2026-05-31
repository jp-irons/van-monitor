#pragma once

#include "lvgl.h"

/**
 * Theme — shared dark-UI colour constants for the van monitor display.
 *
 * All values are expressed as lv_color_hex() calls so they are resolved at
 * compile time and work with both LVGL v8 and v9.
 *
 * Alarm thresholds (apply to both water level and battery SOC):
 *   LOW  : value <= ALARM_LOW_PCT  → amber arc/bar + warning banner
 *   HIGH : value >= ALARM_HIGH_PCT → amber arc/bar + warning banner
 *           (battery high is a positive state, no alarm — handled in DashboardScreen)
 */

namespace display::theme {

// ── Alarm thresholds ────────────────────────────────────────────────────────
static constexpr float ALARM_LOW_PCT  = 10.0f;
static constexpr float ALARM_HIGH_PCT = 90.0f;

// ── Level alarm thresholds (degrees) ────────────────────────────────────────
static constexpr float LEVEL_WARN_DEG = 1.0f;   // amber
static constexpr float LEVEL_ERR_DEG  = 3.0f;   // red

// ── Layout constants (pixels) ────────────────────────────────────────────────
static constexpr int LCD_W        = 240;
static constexpr int LCD_H        = 320;
static constexpr int HEADER_H     = 28;
static constexpr int BANNER_H     = 20;
static constexpr int NAV_H        = 36;

// ── Colours ──────────────────────────────────────────────────────────────────

// Backgrounds
inline lv_color_t BG()       { return lv_color_hex(0x0d1117); }
inline lv_color_t SURFACE()  { return lv_color_hex(0x111827); }
inline lv_color_t BORDER()   { return lv_color_hex(0x1f2937); }
inline lv_color_t CONTROL()  { return lv_color_hex(0x374151); }  // +/- buttons, surfaces

// Text
inline lv_color_t TEXT_PRI() { return lv_color_hex(0xe2e8f0); }  // primary values
inline lv_color_t TEXT_SEC() { return lv_color_hex(0xb8c4d0); }  // labels / keys
inline lv_color_t TEXT_MUT() { return lv_color_hex(0x9ca3af); }  // hints / units

// Semantic accents
inline lv_color_t CYAN()       { return lv_color_hex(0x38bdf8); }  // water level (normal)
inline lv_color_t GREEN()      { return lv_color_hex(0x4ade80); }  // battery (normal / good)
inline lv_color_t AMBER()      { return lv_color_hex(0xf97316); }  // near-empty / near-full (alarm backgrounds)
inline lv_color_t RED()        { return lv_color_hex(0xef4444); }  // critical (alarm backgrounds)

// Level-widget accent colours — brighter than AMBER/RED for visibility on dark bg
inline lv_color_t LEVEL_WARN() { return lv_color_hex(0xfde047); }  // bright yellow-amber (1–3°)
inline lv_color_t LEVEL_ERR()  { return lv_color_hex(0xf87171); }  // light red (>3°)
inline lv_color_t BLUE()     { return lv_color_hex(0x1d6fa8); }  // Empty button bg
inline lv_color_t BLUE_BDR() { return lv_color_hex(0x60b4f0); }  // Empty button border
inline lv_color_t DARK_GRN() { return lv_color_hex(0x166534); }  // Full button bg
inline lv_color_t GRN_BDR()  { return lv_color_hex(0x4ade80); }  // Full button border
inline lv_color_t TEAL()     { return lv_color_hex(0x134e4a); }  // Flat tilt button bg
inline lv_color_t TEAL_BDR() { return lv_color_hex(0x2dd4bf); }  // Flat tilt button border

// Track / inactive areas
inline lv_color_t TRACK()    { return lv_color_hex(0x1f2937); }

// Alarm text colour (used on coloured alarm backgrounds)
inline lv_color_t BLACK()    { return lv_color_hex(0x000000); }

// ── Opacity helpers ──────────────────────────────────────────────────────────
static constexpr lv_opa_t OPA_FULL = LV_OPA_COVER;
static constexpr lv_opa_t OPA_NONE = LV_OPA_TRANSP;

} // namespace display::theme
