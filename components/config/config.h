#pragma once

// Hardware configuration for the oscilloscope project
// bunch of mapping GPIO pins
// BUT also other hardware related settings

//---------- LCD ----------//
// The MSP1308 needs a GPIO pin for reset.
#define HW_LCD_MISO -1
#define HW_LCD_MOSI 23
#define HW_LCD_SCLK 18
#define HW_LCD_CS    5
#define HW_LCD_DC    4
#define HW_LCD_RST  -1
#define HW_LCD_BL   14

// esp-idf/components/driver/spi/include/driver/spi_master.h (v5.2, Line:29)
// #define SPI_MASTER_FREQ_40M (80 * 1000 * 1000 / 2) ///< 40MHz

#define HW_LCD_SPI_HOST SPI2_HOST
#define HW_LCD_SPI_FREQ SPI_MASTER_FREQ_40M // MHz

#define HW_LCD_INV 1
#define HW_LCD_DIR 0

#define HW_LCD_W 320
#define HW_LCD_H 240
#define HW_LCD_OFFSETX 0
#define HW_LCD_OFFSETY 0

#define HW_LCD_DRIVER 0

//---------- Buttons ----------//
#define BTN_A      32
#define BTN_B      33
#define BTN_MENU   13
#define BTN_OPTION  0
#define BTN_SELECT 27
#define BTN_START  39

//---------- Joystick ----------//
#define HW_JOY_X            34
#define HW_JOY_Y            35
#define AVG_ALPHA           0.15f
#define DEADZONE            60
#define JOY_SENSITIVITY     25.0f  // adjust joystick sensitivity for cursor movement

//---------- SD card ----------//
#define HW_SD_MISO 19
#define HW_SD_MOSI 23
#define HW_SD_CLK  18
#define HW_SD_CS   22

// esp-idf/components/driver/sdmmc/include/driver/sdmmc_types.h (v5.2, Line:181)
// #define SDMMC_FREQ_DEFAULT 20000 /*!< SD/MMC Default speed (limited by clock divider) */

#define HW_SD_SPI_HOST SPI2_HOST
#define HW_SD_SPI_FREQ SDMMC_FREQ_DEFAULT

// --------- COLORS ----------//
#define WAVEFORM_COLOR              BLUE
#define FROZEN_TXT_COLOR            BLACK
#define TIMEBASE_TXT_COLOR         BLACK
#define CURSOR_COLOR                RED
#define BACKGROUND_COLOR            WHITE
#define GRID_COLOR                  BLACK
#define VOLTAGE_TXT_COLOR           BLACK

#define NUM_GRID_LINES              5
// grid line macro for drawing grid_line(n)
#define GRID_LINE_VERTICAL(n) ((n) * LCD_W / (NUM_GRID_LINES+1))
#define GRID_LINE_HORIZONTAL(n) ((n) * LCD_H / (NUM_GRID_LINES+1))

// ---------- ADC ----------//
#define SAMPLE_RATE_HZ          10000 // 10kHz sample rate
#define ADC_QUEUE_LENGTH        1024
#define ADC_MIDPOINT            (4096 / 2)
#define LCD_MID_HORIZONTAL      (HW_LCD_H / 2)
#define ADC_CHANNEL             ADC_CHANNEL_2 // GPIO2 (on 330 board schematic its IO2) ONLY WORKS AS ADC if wifi is off
#define ADC_BUFFER_SIZE         8192 // was 4096 but testing a change...
#define SAMPLE_BUFFER_SIZE      ADC_BUFFER_SIZE 

#define DRAW_POINTS             HW_LCD_W   // 1 pixel per sample
#define TIMER_RESOLUTION_HZ     1000000 // 1MHz timer resolution
#define ALARM_INTERVAL_US       (TIMER_RESOLUTION_HZ / SAMPLE_RATE_HZ) // so at 10kHz, this is 100 us
#define FRAME_PERIOD_MS         16 // 16 = 30FPS speed for cursor updates and waveform

