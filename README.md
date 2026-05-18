# Van Monitor

ESP32-S3 monitoring application for a campervan, built on the [embedded-framework](./framework) submodule.

## Hardware

**SpotPear ESP32-S3-Touch-LCD-2**

| Peripheral | Part |
|---|---|
| Display | ST7789T3 — 240 × 320, SPI |
| Touch | CST816D — capacitive, I²C |
| IMU | QMI8658 — 6-axis, I²C |

https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2

## Features

### Implemented

- **Internal temperature** — ESP32-S3 internal sensor exposed at `GET /app/api/temperature` → `{"celsius": 42.5}`
- **Embedded web UI** — served from LittleFS; root `/` redirects to `/app/ui/`
- All framework features: Wi-Fi provisioning, mDNS, OTA with rollback, per-device TLS, NVS-backed network store

### Planned

1. **Water level sensor** — 4–20 mA current loop, 0–2 m range, shunt resistor to ADC
   - 2-point calibration (Empty / Full buttons at known states)
   - Formula: `level_m = (voltage - 0.6) / 2.4 * 2.0`
2. **Touch display UI** — LVGL on ST7789T3, showing water level, battery SOC, and solar yield
   - Frame buffers in PSRAM; DMA buffers in internal SRAM
3. **Venus OS MQTT** — subscribe to battery SOC and solar yield from a Venus OS broker (port 1883)
   - Publish water level; send keepalive every ~60 s to `R/<portal_id>/keepalive`
4. **OTA pull** — pull firmware from GitHub Releases, triggered via MQTT topic

## Repository layout

```
CMakeLists.txt          Top-level ESP-IDF project
partitions.csv          Flash partition table (source of truth)
sdkconfig               Committed build configuration (source of truth)
sdkconfig.defaults      Regeneration baseline
version.txt             Semver string read by ESP-IDF into PROJECT_VER

framework/              Embedded-framework git submodule
  components/           Framework components (auth, OTA, Wi-Fi, HTTP, …)
  docs/                 Framework documentation

main/                   Application code
  app_main.cpp          Entry point
  ApplicationContext     Wires app handlers into the framework
  LoggingConfig          Per-tag log level setup
  app/
    TemperatureHandler   GET /app/api/temperature
  app_files/
    AppFileTable         Embedded file registry
    files/               Web UI assets (HTML, CSS, JS, favicon)
```

## Partition layout

```
nvs         data  nvs       0x009000    24 KB   NVS key-value store
otadata     data  ota       0x00F000     8 KB   OTA boot-slot selection
factory     app   factory   0x020000     4 MB   Factory image (USB flash only)
ota_0       app   ota_0     0x420000     4 MB   OTA slot 0
ota_1       app   ota_1     0x820000     4 MB   OTA slot 1
assets_fs   data  littlefs  0xC20000     2 MB   Embedded web UI and static assets
```

## Getting started

See [`framework/docs/new-machine-setup.md`](framework/docs/new-machine-setup.md) for a full guide to cloning, IDE import, build, and flash.

### Clone (with submodule)

```sh
git clone --recurse-submodules <repo-url>
```

Or if already cloned:

```sh
git submodule update --init --recursive
```

### Build

```sh
idf.py build
```

The post-build step copies `build/my_app.bin` to `build/my_app-<version>.bin` automatically.

### Flash

```sh
idf.py flash monitor
```

### OTA update

Upload `build/my_app-<version>.bin` via the firmware page in the web UI, or:

```sh
curl -X POST https://<device>.local/framework/api/firmware/upload \
     -H "Content-Type: application/octet-stream" \
     --data-binary @build/my_app-<version>.bin
```

### Bump version

Edit `version.txt`, then rebuild. The new version string is embedded in the binary and reflected in the OTA filename.

## API routes

| Method | Path | Description |
|---|---|---|
| GET | `/app/api/temperature` | Internal chip temperature in °C |

Framework routes (provisioning, OTA, device info, etc.) are documented in [`framework/docs/api-reference.md`](framework/docs/api-reference.md).

## Adding a new API handler

1. Declare the handler class in `main/app/`.
2. Add it as a member of `ApplicationContext` (after `fw_` in declaration order).
3. Register it in `ApplicationContext::start()` with `fw_.addRoute(...)`.

See `TemperatureHandler` as a reference.
