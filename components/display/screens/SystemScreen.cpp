#include "SystemScreen.hpp"
#include "Theme.hpp"
#include "DisplayContext.hpp"

#include <cstdio>

namespace display {

using namespace theme;

static constexpr int ROW_H   = 34;   // height of each info row
static constexpr int ROW_X   =  8;   // left margin
static constexpr int ROW_W   = LCD_W - ROW_X * 2;
static constexpr int FIRST_Y = HEADER_H + 6;  // top of first row

static void navCb(lv_event_t* e) {
    auto* ctx = static_cast<DisplayContext*>(lv_event_get_user_data(e));
    ctx->nextPage();
}

// ── create ────────────────────────────────────────────────────────────────────

void SystemScreen::create(DisplayContext* ctx) {
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
    lv_label_set_text(title, "SYSTEM");
    lv_obj_set_style_text_color(title, TEXT_SEC(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    // ── Info rows ─────────────────────────────────────────────────────────
    // Each row: surface card | key label | value label | optional badge
    int y = FIRST_Y;

    // Wi-Fi
    lv_obj_t* rowWifi = makeRow(screen_, y, "Wi-Fi", CYAN());
    valWifi_   = lv_label_create(rowWifi);
    lv_label_set_text(valWifi_, "-");
    lv_obj_set_style_text_color(valWifi_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(valWifi_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(valWifi_, LV_ALIGN_LEFT_MID, 62, 0);
    lv_obj_set_width(valWifi_, 100);
    lv_label_set_long_mode(valWifi_, LV_LABEL_LONG_CLIP);
    badgeWifi_ = makeBadge(rowWifi, false);
    y += ROW_H + 4;

    // IP
    lv_obj_t* rowIp = makeRow(screen_, y, "IP", lv_color_hex(0x818cf8));
    valIp_ = lv_label_create(rowIp);
    lv_label_set_text(valIp_, "-");
    lv_obj_set_style_text_color(valIp_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(valIp_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(valIp_, LV_ALIGN_LEFT_MID, 62, 0);
    y += ROW_H + 4;

    // Hostname
    lv_obj_t* rowHost = makeRow(screen_, y, "Hostname", TEXT_SEC());
    valHost_ = lv_label_create(rowHost);
    lv_label_set_text(valHost_, "-");
    lv_obj_set_style_text_color(valHost_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(valHost_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(valHost_, LV_ALIGN_LEFT_MID, 62, 0);
    lv_obj_set_width(valHost_, 165);
    lv_label_set_long_mode(valHost_, LV_LABEL_LONG_CLIP);
    y += ROW_H + 4;

    // MQTT
    lv_obj_t* rowMqtt = makeRow(screen_, y, "MQTT", lv_color_hex(0xfbbf24));
    valMqtt_ = lv_label_create(rowMqtt);
    lv_label_set_text(valMqtt_, "-");
    lv_obj_set_style_text_color(valMqtt_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(valMqtt_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(valMqtt_, LV_ALIGN_LEFT_MID, 62, 0);
    lv_obj_set_width(valMqtt_, 100);
    lv_label_set_long_mode(valMqtt_, LV_LABEL_LONG_CLIP);
    badgeMqtt_ = makeBadge(rowMqtt, false);
    y += ROW_H + 4;

    // Venus OS
    lv_obj_t* rowVenus = makeRow(screen_, y, "Venus OS", TEXT_MUT());
    valVenus_ = lv_label_create(rowVenus);
    lv_label_set_text(valVenus_, "-");
    lv_obj_set_style_text_color(valVenus_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(valVenus_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(valVenus_, LV_ALIGN_LEFT_MID, 62, 0);
    lv_obj_set_width(valVenus_, 100);
    lv_label_set_long_mode(valVenus_, LV_LABEL_LONG_CLIP);
    badgeVenus_ = makeBadge(rowVenus, false);
    y += ROW_H + 4;

    // Uptime
    lv_obj_t* rowUptime = makeRow(screen_, y, "Uptime", TEXT_MUT());
    valUptime_ = lv_label_create(rowUptime);
    lv_label_set_text(valUptime_, "-");
    lv_obj_set_style_text_color(valUptime_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(valUptime_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(valUptime_, LV_ALIGN_LEFT_MID, 62, 0);
    y += ROW_H + 4;

    // Firmware
    lv_obj_t* rowFw = makeRow(screen_, y, "Firmware", TEXT_MUT());
    valFw_ = lv_label_create(rowFw);
    lv_label_set_text(valFw_, "-");
    lv_obj_set_style_text_color(valFw_, TEXT_PRI(), LV_PART_MAIN);
    lv_obj_set_style_text_font(valFw_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(valFw_, LV_ALIGN_LEFT_MID, 62, 0);

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

void SystemScreen::show() {
    lv_screen_load(screen_);
}

// ── update ────────────────────────────────────────────────────────────────────

void SystemScreen::update(const SystemData& data) {
    char buf[64];

    // Wi-Fi
    snprintf(buf, sizeof(buf), "%s  %d dBm", data.ssid, data.rssi);
    lv_label_set_text(valWifi_, buf);
    lv_obj_set_style_bg_color(badgeWifi_,
        data.rssi > -80 ? lv_color_hex(0x14532d) : lv_color_hex(0x4c1d1d),
        LV_PART_MAIN);
    lv_obj_t* badgeWifiLbl = lv_obj_get_child(badgeWifi_, 0);
    lv_label_set_text(badgeWifiLbl, data.rssi > -80 ? "OK" : "WEAK");

    // IP
    lv_label_set_text(valIp_, data.ipAddr);

    // Hostname
    lv_label_set_text(valHost_, data.hostname);

    // MQTT
    lv_label_set_text(valMqtt_, data.mqttBrokerIp && data.mqttBrokerIp[0] ? data.mqttBrokerIp : "-");
    lv_obj_set_style_bg_color(badgeMqtt_,
        data.mqttOk ? lv_color_hex(0x14532d) : lv_color_hex(0x4c1d1d),
        LV_PART_MAIN);
    lv_obj_t* badgeMqttLbl = lv_obj_get_child(badgeMqtt_, 0);
    lv_label_set_text(badgeMqttLbl, data.mqttOk ? "OK" : "ERR");

    // Venus OS
    lv_label_set_text(valVenus_, data.venusPortalId);
    lv_obj_set_style_bg_color(badgeVenus_,
        data.venusOk ? lv_color_hex(0x14532d) : lv_color_hex(0x4c1d1d),
        LV_PART_MAIN);
    lv_obj_t* badgeVenusLbl = lv_obj_get_child(badgeVenus_, 0);
    lv_label_set_text(badgeVenusLbl, data.venusOk ? "OK" : "ERR");

    // Uptime
    uint32_t d = data.uptimeS / 86400;
    uint32_t h = (data.uptimeS % 86400) / 3600;
    uint32_t m = (data.uptimeS % 3600) / 60;
    snprintf(buf, sizeof(buf), "%lud %luh %lum",
             (unsigned long)d, (unsigned long)h, (unsigned long)m);
    lv_label_set_text(valUptime_, buf);

    // Firmware
    lv_label_set_text(valFw_, data.firmwareVersion);
}

// ── Private helpers ───────────────────────────────────────────────────────────

lv_obj_t* SystemScreen::makeRow(lv_obj_t* parent, int yPos,
                                 const char* key, lv_color_t /*iconColor*/) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, ROW_W, ROW_H);
    lv_obj_set_pos(row, ROW_X, yPos);
    lv_obj_set_style_bg_color(row, SURFACE(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* keyLbl = lv_label_create(row);
    lv_label_set_text(keyLbl, key);
    lv_obj_set_style_text_color(keyLbl, TEXT_MUT(), LV_PART_MAIN);
    lv_obj_set_style_text_font(keyLbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_width(keyLbl, 58);
    lv_obj_align(keyLbl, LV_ALIGN_LEFT_MID, 6, 0);
    lv_label_set_long_mode(keyLbl, LV_LABEL_LONG_CLIP);

    return row;
}

lv_obj_t* SystemScreen::makeBadge(lv_obj_t* parent, bool ok) {
    lv_obj_t* badge = lv_obj_create(parent);
    lv_obj_set_size(badge, 36, 16);
    lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(badge,
        ok ? lv_color_hex(0x14532d) : lv_color_hex(0x4c1d1d),
        LV_PART_MAIN);
    lv_obj_set_style_bg_opa(badge, OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(badge, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(badge, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(badge, 0, LV_PART_MAIN);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(badge);
    lv_label_set_text(lbl, ok ? "OK" : "ERR");
    lv_obj_set_style_text_color(lbl,
        ok ? lv_color_hex(0x86efac) : lv_color_hex(0xfca5a5),
        LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return badge;
}

} // namespace display
