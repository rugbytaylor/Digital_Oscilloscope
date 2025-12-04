#include "waveform_display.h"
#include <string.h>
#include "lvgl.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "DISPLAY";

// Display pins
#define LCD_MOSI 23
#define LCD_CLK  18
#define LCD_DC    4
#define LCD_CS    5
#define LCD_RST  -1
#define LCD_BK   14

#define LCD_HRES 320
#define LCD_VRES 240

static spi_device_handle_t spi;

// LVGL draw buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LCD_HRES * 20];

// Chart + data
static lv_obj_t *chart;
static lv_chart_series_t *series;

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(1); // increment LVGL tick by 1ms
}


/*-------------------------------------------------------------------*/
// LCD SPI Low-Level
/*-------------------------------------------------------------------*/
static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(LCD_DC, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_transmit(spi, &t);
}

static void lcd_data(const void *data, size_t len)
{
    gpio_set_level(LCD_DC, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_transmit(spi, &t);
}

static void lcd_set_address(int x1, int y1, int x2, int y2)
{
    uint8_t data[4];

    lcd_cmd(0x2A);
    data[0] = x1 >> 8; data[1] = x1;
    data[2] = x2 >> 8; data[3] = x2;
    lcd_data(data, 4);

    lcd_cmd(0x2B);
    data[0] = y1 >> 8; data[1] = y1;
    data[2] = y2 >> 8; data[3] = y2;
    lcd_data(data, 4);

    lcd_cmd(0x2C);
}

/*-------------------------------------------------------------------*/
// LCD Init (Manual ILI9341)
/*-------------------------------------------------------------------*/
static void ili9341_init(void)
{
    lcd_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_cmd(0x36);
    uint8_t madctl[1] = {0xA8};  // Landscape rotation
    lcd_data(madctl, 1);

    lcd_cmd(0x3A);
    uint8_t fmt[1] = {0x55};     // 16-bit PIX
    lcd_data(fmt, 1);

    lcd_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_cmd(0x29);
}

/*-------------------------------------------------------------------*/
// SPI Init
/*-------------------------------------------------------------------*/
static void lcd_init(void)
{
    gpio_set_direction(LCD_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_BK, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BK, 1);

    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_HRES * LCD_VRES * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .spics_io_num = LCD_CS,
        .queue_size = 10,
        .mode = 0,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));

    ili9341_init();
    ESP_LOGI(TAG, "ILI9341 Ready");
}

/*-------------------------------------------------------------------*/
// LVGL Flush → SPI write
/*-------------------------------------------------------------------*/
static void lvgl_flush(lv_disp_drv_t *drv,
                       const lv_area_t *area,
                       lv_color_t *color_map)
{
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    int size = w * h;

    lcd_set_address(area->x1, area->y1, area->x2, area->y2);

    static uint16_t buf[LCD_HRES * 20];
    for (int i = 0; i < size; i++)
    {
        uint16_t c = color_map[i].full;
        buf[i] = (c >> 8) | (c << 8);
    }

    lcd_data(buf, size * 2);
    lv_disp_flush_ready(drv);
}

/*-------------------------------------------------------------------*/
// Display + Chart Setup
/*-------------------------------------------------------------------*/
void waveform_display_init(void)
{
    lcd_init();

    lv_init();

    // Create LVGL tick timer - 1ms
    static esp_timer_handle_t lvgl_tick_timer;
    const esp_timer_create_args_t lvgl_tick_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 1000)); // 1 ms

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, LCD_HRES * 20);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_HRES;
    disp_drv.ver_res = LCD_VRES;
    disp_drv.flush_cb = lvgl_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    chart = lv_chart_create(lv_scr_act());
    lv_obj_set_size(chart, LCD_HRES, LCD_VRES);
    lv_obj_center(chart);

    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_chart_set_point_count(chart, LCD_HRES);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 4095);

    series = lv_chart_add_series(chart,
                                 lv_palette_main(LV_PALETTE_DEEP_PURPLE),
                                 LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS);

    for (int i = 0; i < LCD_HRES; i++)
        series->y_points[i] = 2048;

    lv_chart_refresh(chart);

    ESP_LOGI(TAG, "Display + Chart Ready");
}


/*-------------------------------------------------------------------*/
// Add ADC sample → waveform scrolls
/*-------------------------------------------------------------------*/
void waveform_display_add_sample(uint16_t sample)
{
    lv_chart_set_next_value(chart, series, sample);
}

/*-------------------------------------------------------------------*/
// LVGL Task Loop
/*-------------------------------------------------------------------*/
void lvgl_task(void *arg)
{
    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
