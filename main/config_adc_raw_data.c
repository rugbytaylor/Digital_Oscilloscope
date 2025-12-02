/* ------------------------------------------------------------
    ADC Logger → SD Card (5kHz)
    Uses working SD card pins:
        CS=GPIO22, MOSI=23, MISO=19, SCK=18
    Button on GPIO32 starts/stops logging.
    Saves: /sdcard/data.csv
------------------------------------------------------------ */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_wifi.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

#define SAMPLE_RATE_HZ   5000
#define TIMER_RES_HZ     1000000
#define ALARM_US         (TIMER_RES_HZ / SAMPLE_RATE_HZ)

#define MAX_SAMPLES      20000          // 4 seconds at 5 kHz
#define ADC_CHANNEL      ADC_CHANNEL_2  // GPIO 2 (ADC2)
#define BUTTON_GPIO      32

// Working SD pins:
#define SD_CS_PIN        22
#define SD_MOSI_PIN      23
#define SD_MISO_PIN      19
#define SD_SCK_PIN       18

static uint16_t samples[MAX_SAMPLES];
static volatile int sample_index = 0;
static volatile bool logging = false;

static adc_oneshot_unit_handle_t adc_handle;
static QueueHandle_t trigger_q;

/* ---------------- ISR (5kHz timer) ---------------- */
IRAM_ATTR static bool timer_cb(gptimer_handle_t timer,
                               const gptimer_alarm_event_data_t *edata,
                               void *user_ctx)
{
    if (!logging || sample_index >= MAX_SAMPLES) return false;

    uint32_t t = 1;
    BaseType_t hp = pdFALSE;
    xQueueSendFromISR(trigger_q, &t, &hp);

    return hp == pdTRUE;
}

/* ---------------- Sampler Task ---------------- */
void sampler_task(void *arg)
{
    uint32_t dummy;
    while (1) {
        if (xQueueReceive(trigger_q, &dummy, portMAX_DELAY)) {
            int raw = 0;
            adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
            samples[sample_index++] = (uint16_t)raw;
        }
    }
}

/* ---------------- Button Helpers ---------------- */
bool button_pressed() {
    return gpio_get_level(BUTTON_GPIO) == 0;
}

void wait_release() {
    while (button_pressed()) vTaskDelay(pdMS_TO_TICKS(20));
}

/* ---------------- SD Card Init ---------------- */
esp_err_t init_sdcard()
{
    printf("Mounting SD card...\n");

    // SPI Bus Config
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI_PIN,
        .miso_io_num = SD_MISO_PIN,
        .sclk_io_num = SD_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    sdspi_device_config_t devcfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    devcfg.host_id = SPI3_HOST;
    devcfg.gpio_cs = SD_CS_PIN;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
    };

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &devcfg, &mount_cfg, &card);

    if (ret != ESP_OK) {
        printf("SD mount failed! (%s)\n", esp_err_to_name(ret));
    }

    return ret;
}

/* ---------------- MAIN ---------------- */
void app_main(void)
{
    // Required because ADC2 conflicts with WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    printf("\nSD + ADC Logger Ready\n");

    /* ------------- Button ------------- */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&io);

    /* ------------- ADC ------------- */
    adc_oneshot_unit_init_cfg_t icfg = { .unit_id = ADC_UNIT_2 };
    adc_oneshot_new_unit(&icfg, &adc_handle);

    adc_oneshot_chan_cfg_t ccfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &ccfg);

    /* ------------- Timer (5kHz) ------------- */
    gptimer_handle_t timer;
    gptimer_config_t tcfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_RES_HZ
    };
    gptimer_new_timer(&tcfg, &timer);

    gptimer_alarm_config_t acfg = {
        .alarm_count = ALARM_US,
        .flags.auto_reload_on_alarm = true
    };
    gptimer_set_alarm_action(timer, &acfg);

    gptimer_event_callbacks_t cb = { .on_alarm = timer_cb };
    gptimer_register_event_callbacks(timer, &cb, NULL);

    gptimer_enable(timer);
    gptimer_start(timer);

    /* ------------- Queue + Sampling Task ------------- */
    trigger_q = xQueueCreate(32, sizeof(uint32_t));
    xTaskCreate(sampler_task, "sampler", 4096, NULL, 4, NULL);

    /* ---------------- WAIT FOR START ---------------- */
    printf("Press button to START logging...\n");
    while (!button_pressed()) vTaskDelay(5);
    wait_release();

    sample_index = 0;
    logging = true;

    printf("Logging started...\n");

    /* ---------------- RUN UNTIL STOP ---------------- */
    while (!button_pressed() && sample_index < MAX_SAMPLES)
        vTaskDelay(1);

    logging = false;
    wait_release();

    int count = sample_index;
    printf("Logging stopped. %d samples captured.\n", count);

    /* ---------------- SAVE TO SD ---------------- */
    if (init_sdcard() != ESP_OK) return;

    FILE *f = fopen("/sdcard/data.csv", "w");
    if (!f) {
        printf("File open failed!\n");
        return;
    }

    fprintf(f, "time_s,adc_raw\n");

    for (int i = 0; i < count; i++) {
        float t = (float)i / SAMPLE_RATE_HZ;
        fprintf(f, "%f,%d\n", t, samples[i]);
    }

    fclose(f);

    printf("Saved /sdcard/data.csv\n");
}
