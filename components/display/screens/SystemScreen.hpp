#pragma once

#include "lvgl.h"

namespace display {

class DisplayContext;
struct SystemData;

/**
 * SystemScreen — page 2: connection status and device identity.
 *
 * Layout (240 × 320):
 *   [0  – 27]  Header bar: "SYSTEM"
 *   [28 – 283] Info rows: Wi-Fi, IP, Hostname, MQTT, Venus OS, Uptime, Firmware
 *   [284– 319] Nav button
 */
class SystemScreen {
public:
    SystemScreen() = default;

    void create(DisplayContext* ctx);
    void show();
    void update(const SystemData& data);

private:
    lv_obj_t* makeRow(lv_obj_t* parent, int yPos,
                      const char* key, lv_color_t iconColor);
    lv_obj_t* makeBadge(lv_obj_t* parent, bool ok);

    lv_obj_t* screen_ {nullptr};

    // Value labels — updated in update()
    lv_obj_t* valWifi_    {nullptr};
    lv_obj_t* badgeWifi_  {nullptr};
    lv_obj_t* valIp_      {nullptr};
    lv_obj_t* valHost_    {nullptr};
    lv_obj_t* valMqtt_    {nullptr};
    lv_obj_t* badgeMqtt_  {nullptr};
    lv_obj_t* valVenus_   {nullptr};
    lv_obj_t* badgeVenus_ {nullptr};
    lv_obj_t* valUptime_  {nullptr};
    lv_obj_t* valFw_      {nullptr};
};

} // namespace display
