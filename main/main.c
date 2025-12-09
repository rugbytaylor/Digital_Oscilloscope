#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_adc/adc_oneshot.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_wifi.h" //need to ensure wifi is off to use ADC on GPIO2
#include "esp_timer.h"
#include "driver/gpio.h"

#include "LUT.h"
#include "lcd.h"
#include "config.h" 
#include "waveform_display.h"
#include "joystick_dma.h"
#include "btns.h"

static const char *TAG = "lab7";

// global stuff
QueueHandle_t adc_queue;
volatile uint16_t adc_buff[ADC_BUFFER_SIZE];
volatile size_t adc_index = 0;
adc_oneshot_unit_handle_t adc_handle;
gptimer_handle_t gptimer;
TaskHandle_t main_task_handle = NULL;
TimerHandle_t frame_timer;
uint8_t frame_count = 0;
joystick_pos_t joystick_pos;


static void frame_timer_cb(TimerHandle_t xTimer) 
{ 
    if (main_task_handle != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(main_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// ADC Timer ISR
IRAM_ATTR static bool timer_callback(gptimer_handle_t timer,
                                     const gptimer_alarm_event_data_t *edata,
                                     void *user_ctx)
{
    uint16_t sample = 0;
    adc_oneshot_read(adc_handle, ADC_CHANNEL, (int*) &sample);

    BaseType_t hp = pdFALSE;
    xQueueSendFromISR(adc_queue, &sample, &hp);
    return (hp == pdTRUE);
}

// ADC Queue Consumer → Store in Circular Buffer
void adc_sample_task(void *arg)
{
    uint16_t val;
    while (1)
    {
        if (xQueueReceive(adc_queue, &val, portMAX_DELAY))
        {
            adc_buff[adc_index] = val;
            adc_index = (adc_index + 1) % ADC_BUFFER_SIZE;

            waveform_display_add_sample(val);
        }
    }
}

// Button Config
static void config_btns(void)
{
    button_init(BTN_A);
    button_init(BTN_B);
    button_init(BTN_MENU);
}

// config structs
static const adc_oneshot_unit_init_cfg_t adc_init_cfg = {
    .unit_id = ADC_UNIT_2
};
static const adc_oneshot_chan_cfg_t adc_chan_cfg = {
    .bitwidth = ADC_BITWIDTH_12,
    .atten = ADC_ATTEN_DB_12
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


void app_main(void)
{
    ESP_LOGI(TAG, "Starting Digital Oscilloscope...");
    main_task_handle = xTaskGetCurrentTaskHandle();

    // init setups
    esp_wifi_stop(); // force wifi off to use ADC2
    esp_wifi_deinit(); // ^^
    lcd_init();
    joystick_init();
    waveform_display_init();
    config_btns();

    // create and start frame timer
    frame_timer = xTimerCreate(
        "frame_timer",
        pdMS_TO_TICKS(FRAME_PERIOD_MS),
        pdTRUE,
        NULL,
        frame_timer_cb
    );
    xTimerStart(frame_timer, 0);

    // ADC setup
    adc_oneshot_new_unit(&adc_init_cfg, &adc_handle);
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &adc_chan_cfg);
    // ADC queue setup
    adc_queue = xQueueCreate(ADC_QUEUE_LENGTH, sizeof(uint16_t));
    // create ADC task BEFORE starting timer
    xTaskCreate(adc_sample_task, "adc_task", 4096, NULL, 3, NULL);

    // timer setup
    gptimer_new_timer(&timer_cfg, &gptimer);
    gptimer_register_event_callbacks(gptimer, &timer_cbs, NULL); //register ISR
    gptimer_set_alarm_action(gptimer, &alarm_cfg); // setup alarm
    gptimer_enable(gptimer);
    gptimer_start(gptimer);

    // Button state tracking for debouncing
    bool btn_a_prev = false;
    bool btn_b_prev = false;
    bool btn_menu_prev = false;
    bool frozen = false;

    // main display loop
    while (1)
    {
        // Wait for frame timer notification for precise timing
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        bool btn_a = btn_pressed(BTN_A);
        bool btn_b = btn_pressed(BTN_B);
        bool btn_menu = btn_pressed(BTN_MENU);

        // bnt A pressed so freeze screen 
        if (btn_a && !btn_a_prev) {
            frozen = true;
            lcd_drawString(5, 5, "SCREEN FROZEN", FROZEN_TXT_COLOR);
            // reset joystick pos to center
            joystick_pos.y = 0;
        }

        // btn B pressed so unfreeze if
        if (frozen && btn_b && !btn_b_prev) {
            frozen = false;
            lcd_drawString(5, 5, "SCREEN FROZEN", WHITE);
        }

        // btn MENU pressed so cycle timebase and unfreeze
        if (btn_menu && !btn_menu_prev) {
            cycle_timebase_mode();
            frozen = false;
            lcd_drawString(5, 5, "SCREEN FROZEN", WHITE);
        }

        // update joystick every frame if frozen or not
        joystick_read(&joystick_pos);

        // Only draw waveform if not frozen
        if (!frozen) {
            int redraw_interval = get_redraw_interval();
            if (frame_count % redraw_interval == 0) {
                waveform_display_draw_full_frame();
                frame_count = 0;
            }
            frame_count++;
        } else {
            int y_curr = joystick_pos.y;
            cursor_update(frozen, y_curr);
        }
        
        // reset btns
        btn_a_prev = btn_a;
        btn_b_prev = btn_b;
        btn_menu_prev = btn_menu;
    }

}