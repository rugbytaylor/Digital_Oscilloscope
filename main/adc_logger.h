#pragma once

// ==== USER CONFIG ====
#define SAMPLE_RATE_HZ 5000  // 5kHz sample rate
#define TIMER_RES_HZ 1000000 // 1MHz timer resolution
#define ALARM_US ( TIMER_RES_HZ / SAMPLE_RATE_HZ )
#define MAX_SAMPLES 20000         // 4 seconds at 5 kHz
#define ADC_CHANNEL ADC_CHANNEL_2 // GPIO 2 (ADC2)
#define BUTTON_GPIO 32

// Working SD pins:
#define SD_CS_PIN 22
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// Configuration for raw ADC data acquisition
// blocks until button is pressed -> logs -> button pressed again to stop
// then saves csv to SD card
void adc_logger_run( void );
