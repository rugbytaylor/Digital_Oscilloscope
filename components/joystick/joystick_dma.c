#include "joystick_dma.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

static const char *TAG = "joystick_dma";

static adc_oneshot_unit_handle_t adc_handle;
static int baseline_x = 2000;
static int baseline_y = 2000;
static float filt_x = 2000;
static float filt_y = 2000;

static int normalize(int raw, int base)
{
    int d = raw - base;
    if (abs(d) < DEADZONE) return 0;
    float pct = (d / 2048.0f) * 100.0f;
    if (pct > 100) pct = 100;
    if (pct < -100) pct = -100;
    return (int)pct;
}

static void joystick_task(void *arg)
{
    int raw_x, raw_y;
    while (1) {
        adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &raw_x);
        adc_oneshot_read(adc_handle, ADC_CHANNEL_7, &raw_y);
        // low-pass filtering
        filt_x = filt_x + AVG_ALPHA * (raw_x - filt_x);
        filt_y = filt_y + AVG_ALPHA * (raw_y - filt_y);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void joystick_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_7, &cfg));

    // center calibration for joystick using multiple samples
    baseline_x = 0;
    baseline_y = 0;
    for (int i = 0; i < 25; i++) {
        int raw_x, raw_y;
        adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &raw_x);
        adc_oneshot_read(adc_handle, ADC_CHANNEL_7, &raw_y);
        baseline_x += raw_x;
        baseline_y += raw_y;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    baseline_x /= 25;
    baseline_y /= 25;
    ESP_LOGI(TAG, "Joystick baseline set: X=%d Y=%d", baseline_x, baseline_y);
    
    xTaskCreate(joystick_task, "joystick_task", 2048, NULL, 1, NULL);
}

void joystick_read(joystick_pos_t *pos)
{
    pos->y = normalize((int)filt_y, baseline_y);
    pos->x = normalize((int)filt_x, baseline_x);
}

