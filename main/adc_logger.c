#include "adc_logger.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include <stdio.h>

#define TAG "ADC_LOGGER"

// ==== USER CONFIGS ====
#define LOGGER_SAMPLE_RATE_HZ 5000  // 5kHz sample rate
#define LOGGER_TIMER_RES_HZ 1000000 // 1MHz timer resolution
#define LOGGER_ALARM_US ( LOGGER_TIMER_RES_HZ / LOGGER_SAMPLE_RATE_HZ )
#define LOGGER_MAX_SAMPLES 20000         // 4 seconds at 5 kHz
#define LOGGER_ADC_CHANNEL ADC_CHANNEL_2 // GPIO 2 (ADC2)
#define LOGGER_BUTTON_GPIO 32

// Working SD pins:
#define SD_CS_PIN 22
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// ==== Internal State ====
static uint16_t samples[LOGGER_MAX_SAMPLES];
static volatile int sample_index = 0;
static volatile bool logging     = false;

static adc_oneshot_unit_handle_t adc_handle;
static QueueHandle_t trigger_q;

// ==== ISR ====
IRAM_ATTR static bool timer_isr_cb( gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx )
{
  if ( !logging || sample_index >= LOGGER_MAX_SAMPLES ) return false;
  uint32_t trig = 1;
  BaseType_t hp = pdFALSE;
  xQueueSendFromISR( trigger_q, &trig, &hp );
  return ( hp == pdTRUE );
}

// ==== Sampler Task ====
static void sampler_task( void *arg )
{
  uint32_t dummy;
  while ( 1 )
  {
    if ( xQueueReceive( trigger_q, &dummy, portMAX_DELAY ) )
    {
      int raw = 0;
      adc_oneshot_read( adc_handle, LOGGER_ADC_CHANNEL, &raw );
      samples[sample_index++] = (uint16_t)raw;
    }
  }
}

// ==== SD Init ====
static esp_err_t init_sdcard( void )
{
  ESP_LOGI( TAG, "Mounting SD card..." );

  spi_bus_config_t bus_cfg = {
      .mosi_io_num   = SD_MOSI_PIN,
      .miso_io_num   = SD_MISO_PIN,
      .sclk_io_num   = SD_SCK_PIN,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
  };

  ESP_ERROR_CHECK( spi_bus_initialize( SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO ) );

  sdspi_device_config_t devcfg = SDSPI_DEVICE_CONFIG_DEFAULT();
  devcfg.host_id               = SPI3_HOST;
  devcfg.gpio_cs               = SD_CS_PIN;

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot         = SPI3_HOST;

  esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
      .format_if_mount_failed = false,
      .max_files              = 5,
  };

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdspi_mount( "/sdcard", &host, &devcfg, &mount_cfg, &card );

  if ( ret != ESP_OK ) ESP_LOGE( TAG, "SD mount failed: %s", esp_err_to_name( ret ) );

  return ret;
}

// ==== Button Helpers ====
static bool button_pressed( void ) { return gpio_get_level( LOGGER_BUTTON_GPIO ) == 0; }

static void wait_release( void )
{
  while ( button_pressed() )
    vTaskDelay( pdMS_TO_TICKS( 20 ) );
}

// ==== PUBLIC ENTRY FUNCTION ====
void adc_logger_run( void )
{
  ESP_LOGI( TAG, "Starting ADC Logger" );

  esp_wifi_stop();
  esp_wifi_deinit();

  // Button Init
  gpio_config_t io = {
      .pin_bit_mask = 1ULL << LOGGER_BUTTON_GPIO,
      .mode         = GPIO_MODE_INPUT,
      .pull_up_en   = 1,
  };
  gpio_config( &io );

  // ADC Config
  adc_oneshot_unit_init_cfg_t icfg = {
      .unit_id = ADC_UNIT_2,
  };
  adc_oneshot_new_unit( &icfg, &adc_handle );

  adc_oneshot_chan_cfg_t ccfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
  adc_oneshot_config_channel( adc_handle, LOGGER_ADC_CHANNEL, &ccfg );

  // Timer Config
  gptimer_handle_t timer;
  gptimer_config_t tcfg = {
      .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
      .direction     = GPTIMER_COUNT_UP,
      .resolution_hz = LOGGER_TIMER_RES_HZ,
  };
  gptimer_new_timer( &tcfg, &timer );

  gptimer_alarm_config_t acfg = {
      .alarm_count                = LOGGER_ALARM_US,
      .flags.auto_reload_on_alarm = true,
  };
  gptimer_set_alarm_action( timer, &acfg );

  gptimer_event_callbacks_t cb = {
      .on_alarm = timer_isr_cb,
  };
  gptimer_register_event_callbacks( timer, &cb, NULL );

  gptimer_enable( timer );
  gptimer_start( timer );

  trigger_q = xQueueCreate( 32, sizeof( uint32_t ) );
  xTaskCreate( sampler_task, "adc_sampler", 4096, NULL, 4, NULL );

  // Wait for User
  ESP_LOGI( TAG, "Press to START logging..." );
  while ( !button_pressed() )
    vTaskDelay( pdMS_TO_TICKS( 5 ) );
  wait_release();

  sample_index = 0;
  logging      = true;
  ESP_LOGI( TAG, "Logging..." );

  while ( !button_pressed() && sample_index < LOGGER_MAX_SAMPLES )
    vTaskDelay( 1 );

  logging = false;
  wait_release();
  ESP_LOGI( TAG, "Captured %d samples", sample_index );

  if ( init_sdcard() != ESP_OK ) return;

  FILE *f = fopen( "/sdcard/data.csv", "w" );
  if ( !f )
  {
    ESP_LOGE( TAG, "CSV open failed!" );
    return;
  }

  fprintf( f, "time_s,adc_raw\n" );
  for ( int i = 0; i < sample_index; i++ )
    fprintf( f, "%f,%u\n", (float)i / LOGGER_SAMPLE_RATE_HZ, samples[i] );
  fclose( f );

  ESP_LOGI( TAG, "Saved: /sdcard/data.csv" );
}
