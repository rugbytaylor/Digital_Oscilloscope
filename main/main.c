#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_wifi.h" //need to ensure wifi is off to use ADC on GPIO2
#include "LUT.c"
#include "version.h"

static const char *TAG = "lab7";

// constants
#define SAMPLE_RATE_HZ 10000 // 10kHz sample rate
#define TIMER_RESOLUTION_HZ 1000000 // 1MHz timer resolution
#define ALARM_INTERVAL_US (TIMER_RESOLUTION_HZ / SAMPLE_RATE_HZ) // so at 10kHz, this is 100 us
#define ADC_CHANNEL ADC_CHANNEL_2 // GPIO2 (on 330 board schematic its IO2) ONLY WORKS AS ADC if wifi is off
#define ADC_BUFFER_SIZE 4096
#define ADC_QUEUE_LENGTH 1024

// global stuff
QueueHandle_t adc_queue;
volatile uint16_t adc_buff[ADC_BUFFER_SIZE];
volatile size_t adc_index = 0;
adc_oneshot_unit_handle_t adc_handle;
gptimer_handle_t gptimer;

// Forward declare ISR so config can see it
static bool IRAM_ATTR timer_callback(gptimer_handle_t timer,
                                     const gptimer_alarm_event_data_t *edata,
                                     void *user_ctx);

// Config structs 
static const adc_oneshot_unit_init_cfg_t adc_init_cfg = {
    .unit_id = ADC_UNIT_2,
};
static const adc_oneshot_chan_cfg_t adc_chan_cfg = {
    .bitwidth = ADC_BITWIDTH_12,
    .atten = ADC_ATTEN_DB_12,
};
static const gptimer_config_t timer_cfg = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = TIMER_RESOLUTION_HZ,
};
static const gptimer_alarm_config_t alarm_cfg = {
    .alarm_count = ALARM_INTERVAL_US,
    .reload_count = 0,
    .flags.auto_reload_on_alarm = true,
};
static const gptimer_event_callbacks_t timer_cbs = {
    .on_alarm = timer_callback,
};

/* --------------------------------------------------------------
    Functions
-------------------------------------------------------------- */

// Timer ISR to read ADC value
IRAM_ATTR static bool timer_callback(gptimer_handle_t timer, 
                                    const gptimer_alarm_event_data_t *edata, 
                                    void *user_ctx)
{
    int adc_sample = 0;
    adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_sample);
    BaseType_t hp = pdFALSE;
    // send adc sample to queue
    xQueueSendFromISR(adc_queue, &adc_sample, &hp);
    return hp == pdTRUE;   // request context switch
}

// store adc readings from queue into a buffer
void adc_sample_task(void *arg)
{
    int adc_reading;
    while (1) {
        // Wait for ADC reading from ISR
        if (xQueueReceive(adc_queue, &adc_reading, portMAX_DELAY)) {
            // Store adc_reading in circular buffer
            adc_buff[adc_index] = adc_reading;
            adc_index = (adc_index + 1) % ADC_BUFFER_SIZE;
        }
    }
}

void monitor_task(void *arg)
{
    while (1) {
        size_t prev_index = (adc_index - 1 + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;
        uint16_t last_adc_raw = adc_buff[prev_index];
        // retrieve real voltage value from LUT
        float vin_real = ADC_LUT[last_adc_raw];
        // test for correct LUT mapping by logging values
        ESP_LOGI(TAG, "ADC_raw: %d LUT Voltage: %.3f", last_adc_raw, vin_real);
        vTaskDelay(pdMS_TO_TICKS(500)); // 500ms delay between display updates
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Digital Oscilloscope...");
    ESP_LOGI(TAG, "Firmware Version: %d.%d.%d", FW_MAJOR, FW_MINOR, FW_PATCH);
    
    // force wifi off to use ADC2 on GPIO2
    esp_wifi_stop();
    esp_wifi_deinit();

    // ADC setup
    adc_oneshot_new_unit(&adc_init_cfg, &adc_handle);
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &adc_chan_cfg);
    // ADC queue setup
    adc_queue = xQueueCreate(ADC_QUEUE_LENGTH, sizeof(uint16_t));

    // timer setup
    gptimer_new_timer(&timer_cfg, &gptimer);
    gptimer_set_alarm_action(gptimer, &alarm_cfg); // setup alarm
    gptimer_register_event_callbacks(gptimer, &timer_cbs, NULL); //register ISR
    gptimer_enable(gptimer);
    gptimer_start(gptimer);

    // start tasks
    xTaskCreate(adc_sample_task, "adc_task", 4096, NULL, 3, NULL);
    
    // for testing LUT output
    xTaskCreate(monitor_task, "monitor_task", 4096, NULL, 1, NULL);

}
