#include "DisplayContext.hpp"
#include "Theme.hpp"
#include "drivers/cst816d.hpp"

#include "logger/Logger.hpp"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"   // esp_driver_ledc (PRIV_REQUIRES)
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static logger::Logger log{display::DisplayContext::TAG};

// ── GPIO pin assignments (Waveshare ESP32-S3-Touch-LCD-2) ────────────────────
// Source: Waveshare schematic (verified 2026-05-22).
static constexpr gpio_num_t LCD_MOSI = GPIO_NUM_38;
static constexpr gpio_num_t LCD_SCLK = GPIO_NUM_39;
static constexpr gpio_num_t LCD_MISO = GPIO_NUM_NC;   // not used for write-only display
static constexpr gpio_num_t LCD_CS   = GPIO_NUM_45;
static constexpr gpio_num_t LCD_DC   = GPIO_NUM_42;
static constexpr gpio_num_t LCD_RST  = GPIO_NUM_0;
static constexpr gpio_num_t LCD_BL   = GPIO_NUM_1;
static constexpr gpio_num_t TP_SDA   = GPIO_NUM_48;
static constexpr gpio_num_t TP_SCL   = GPIO_NUM_47;
static constexpr gpio_num_t TP_INT   = GPIO_NUM_46;
static constexpr gpio_num_t TP_RST   = GPIO_NUM_0;    // shared reset line with LCD

// ── Backlight LEDC config ─────────────────────────────────────────────────────
static constexpr ledc_mode_t      BL_SPEED_MODE = LEDC_LOW_SPEED_MODE;
static constexpr ledc_timer_t     BL_TIMER      = LEDC_TIMER_0;
static constexpr ledc_channel_t   BL_CHANNEL    = LEDC_CHANNEL_0;
static constexpr uint32_t         BL_FREQ_HZ    = 1000;
static constexpr ledc_timer_bit_t BL_DUTY_RES   = LEDC_TIMER_8_BIT;
static constexpr uint32_t         BL_DUTY_FULL  = 255;   // 100 %
static constexpr uint32_t         BL_DUTY_DIM   = 38;    // ~15 %

// ── Display parameters ────────────────────────────────────────────────────────
static constexpr int    LCD_SPI_HOST    = SPI2_HOST;
static constexpr int    LCD_SPI_HZ     = 40'000'000;  // 40 MHz
static constexpr int    LCD_CMD_BITS   = 8;
static constexpr int    LCD_PARAM_BITS = 8;


namespace display {

// ── Construction / destruction ────────────────────────────────────────────────

DisplayContext::DisplayContext() {
    log.debug("constructor");
}

DisplayContext::~DisplayContext() {
    log.info("destructor");
}

// ── Public API ────────────────────────────────────────────────────────────────

void DisplayContext::start() {
    log.info("start — initialising display stack");
    initSpi();
    initLcd();
    initBacklight();
    initI2c();
    initTouch();
    initLvgl();

    // Create all three screens (they register themselves with LVGL)
    dashboard_.create(this);
    sysInfo_.create(this);
    calibrate_.create(this);

    // Show page 1 by default
    currentPage_ = 0;
    dashboard_.show();

    log.info("display ready");
}

void DisplayContext::loop() {
    // Check for a pending return-to-dashboard request.  The flag is set by
    // scheduleReturnToDashboard() which may be called from the esp_timer task;
    // the actual LVGL call is deferred here where we are on the main-loop task.
    if (returnPending_.exchange(false)) {
        returnToDashboard();
    }
}

void DisplayContext::updateWaterLevel(const WaterData& data) {
    lvgl_port_lock(0);
    dashboard_.updateWater(data);
    calibrate_.updateRaw(data);
    lvgl_port_unlock();
}

void DisplayContext::updateBattery(const BatteryData& data) {
    lvgl_port_lock(0);
    dashboard_.updateBattery(data);
    lvgl_port_unlock();
}

void DisplayContext::updateSystem(const SystemData& data) {
    lvgl_port_lock(0);
    sysInfo_.update(data);
    lvgl_port_unlock();
}

void DisplayContext::setActivityCallback(std::function<void()> cb) {
    onTouchActivity_ = std::move(cb);
}

void DisplayContext::brighten() {
    ESP_ERROR_CHECK(ledc_set_duty(BL_SPEED_MODE, BL_CHANNEL, BL_DUTY_FULL));
    ESP_ERROR_CHECK(ledc_update_duty(BL_SPEED_MODE, BL_CHANNEL));
    log.debug("backlight full");
}

void DisplayContext::dim() {
    ESP_ERROR_CHECK(ledc_set_duty(BL_SPEED_MODE, BL_CHANNEL, BL_DUTY_DIM));
    ESP_ERROR_CHECK(ledc_update_duty(BL_SPEED_MODE, BL_CHANNEL));
    log.debug("backlight dim");
}

void DisplayContext::scheduleReturnToDashboard() {
    returnPending_.store(true);
}

void DisplayContext::returnToDashboard() {
    if (currentPage_ == 0) return;
    currentPage_ = 0;
    lvgl_port_lock(0);
    dashboard_.show();
    lvgl_port_unlock();
    log.debug("returned to dashboard on inactivity");
}

void DisplayContext::nextPage() {
    currentPage_ = (currentPage_ + 1) % PAGE_COUNT;
    lvgl_port_lock(0);
    switch (currentPage_) {
        case 0: dashboard_.show();  break;
        case 1: calibrate_.show(); break;
        case 2: sysInfo_.show();   break;
    }
    lvgl_port_unlock();
    log.debug("page -> %d", currentPage_);
}

// ── Hardware init ─────────────────────────────────────────────────────────────

void DisplayContext::initSpi() {
    log.debug("SPI init");

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num     = LCD_MOSI;
    buscfg.miso_io_num     = LCD_MISO;
    buscfg.sclk_io_num     = LCD_SCLK;
    buscfg.quadwp_io_num   = -1;
    buscfg.quadhd_io_num   = -1;
    buscfg.max_transfer_sz = theme::LCD_W * theme::LCD_H * sizeof(uint16_t);  // worst-case single transfer
    ESP_ERROR_CHECK(spi_bus_initialize(
        static_cast<spi_host_device_t>(LCD_SPI_HOST),
        &buscfg,
        SPI_DMA_CH_AUTO
    ));
}

void DisplayContext::initLcd() {
    log.debug("LCD init (ST7789T3)");

    // ── Panel IO (SPI transport) ──────────────────────────────────────────
    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num       = LCD_CS;
    io_cfg.dc_gpio_num       = LCD_DC;
    io_cfg.spi_mode          = 0;
    io_cfg.pclk_hz           = LCD_SPI_HZ;
    io_cfg.trans_queue_depth = 10;
    io_cfg.lcd_cmd_bits      = LCD_CMD_BITS;
    io_cfg.lcd_param_bits    = LCD_PARAM_BITS;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        static_cast<esp_lcd_spi_bus_handle_t>(LCD_SPI_HOST),
        &io_cfg,
        &ioHandle_
    ));

    // ── ST7789 panel ──────────────────────────────────────────────────────
    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = LCD_RST;
    panel_cfg.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_cfg.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(ioHandle_, &panel_cfg, &panelHandle_));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panelHandle_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panelHandle_));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panelHandle_, true));  // ST7789T3 requires inversion
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panelHandle_, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panelHandle_, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panelHandle_, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panelHandle_, true));
}

void DisplayContext::initBacklight() {
    log.debug("Backlight init (LEDC PWM, GPIO %d)", (int)LCD_BL);

    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode      = BL_SPEED_MODE;
    timer_cfg.timer_num       = BL_TIMER;
    timer_cfg.duty_resolution = BL_DUTY_RES;
    timer_cfg.freq_hz         = BL_FREQ_HZ;
    timer_cfg.clk_cfg         = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {};
    ch_cfg.gpio_num   = LCD_BL;
    ch_cfg.speed_mode = BL_SPEED_MODE;
    ch_cfg.channel    = BL_CHANNEL;
    ch_cfg.timer_sel  = BL_TIMER;
    ch_cfg.duty       = BL_DUTY_FULL;
    ch_cfg.hpoint     = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

void DisplayContext::initI2c() {
    log.debug("I2C init (touch)");

    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port            = I2C_NUM_0;
    bus_cfg.sda_io_num          = TP_SDA;
    bus_cfg.scl_io_num          = TP_SCL;
    bus_cfg.clk_source          = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt   = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2cBus_));
}

void DisplayContext::initTouch() {
    log.debug("Touch init (CST816D)");
    ESP_ERROR_CHECK(cst816d_init(i2cBus_, TP_RST, TP_INT));
}

// ── LVGL init ─────────────────────────────────────────────────────────────────

void DisplayContext::initLvgl() {
    log.debug("LVGL init");

    // ── Initialise port ───────────────────────────────────────────────────
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    // ── Add display — DMA-capable internal SRAM buffers ──────────────────
    // PSRAM cannot be read by the SPI DMA engine on ESP32-S3, so draw buffers
    // must live in internal SRAM with MALLOC_CAP_DMA.  A 40-line buffer uses
    // ~19 KB; double-buffered = ~38 KB total, well within SRAM budget.
    // 20 lines × 240 px × 2 bytes = 9,600 bytes from DMA-capable internal SRAM.
    // Single buffer to minimise DMA heap pressure while WiFi/lwIP is active.
    // Increase DRAW_LINES or enable double_buffer once memory headroom is confirmed.
    static constexpr size_t DRAW_LINES = 20;
    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle           = ioHandle_;
    disp_cfg.panel_handle        = panelHandle_;
    disp_cfg.buffer_size         = theme::LCD_W * DRAW_LINES * sizeof(uint16_t);
    disp_cfg.double_buffer       = false;
    disp_cfg.hres                = static_cast<uint32_t>(theme::LCD_W);
    disp_cfg.vres                = static_cast<uint32_t>(theme::LCD_H);
    disp_cfg.monochrome          = false;
    disp_cfg.rotation.swap_xy    = false;
    disp_cfg.rotation.mirror_x   = false;
    disp_cfg.rotation.mirror_y   = false;
    disp_cfg.flags.buff_dma      = true;   // allocate from DMA-capable internal SRAM
    disp_cfg.flags.buff_spiram   = false;  // PSRAM not DMA-accessible via SPI master
    disp_cfg.flags.swap_bytes    = true;   // RGB565 byte-order fix for Waveshare ST7789
    lvglDisp_ = lvgl_port_add_disp(&disp_cfg);
    if (!lvglDisp_) {
        log.error("lvgl_port_add_disp failed");
        return;
    }

    // ── Register custom touch indev ───────────────────────────────────────
    lvglTouch_ = lv_indev_create();
    lv_indev_set_type(lvglTouch_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvglTouch_, touchReadCb);
    lv_indev_set_user_data(lvglTouch_, this);
    lv_indev_set_display(lvglTouch_, lvglDisp_);
}

// ── Static LVGL input-device callback ────────────────────────────────────────

void DisplayContext::touchReadCb(lv_indev_t* indev, lv_indev_data_t* data) {
    TouchPoint tp = cst816d_read();
    data->point.x = static_cast<int32_t>(tp.x);
    data->point.y = static_cast<int32_t>(tp.y);
    data->state   = tp.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

    if (tp.pressed) {
        auto* self = static_cast<DisplayContext*>(lv_indev_get_user_data(indev));
        if (self && self->onTouchActivity_) {
            self->onTouchActivity_();
        }
    }
}

} // namespace display
