#include "waveform_display.h"
#include "LUT.h"
#include "config.h"
#include "lcd.h"
#include "math.h"
#include "joystick_dma.h"
#include <stdio.h>

#define SLOPE_CONVERSION     17.06f // got this by simple math
#define DECIMATION_AMT       (SAMPLE_BUFFER_SIZE / LCD_W)

static uint16_t sample_buffer[SAMPLE_BUFFER_SIZE];
static volatile uint32_t sample_write_index = 0;
static int wave_x = 0;
static int last_y = -1; // starting cursor y position
static int drawn_y_values[HW_LCD_W]; // to track drawn waveform y values

// ----------------- cursor stuff ------------------------------------
// -------------------------------------------------------------------

// draw a horizontal cursor bar at y
static void draw_cursor(int x, int y, uint16_t color)
{
    (void)x; // unused currently
    lcd_drawHLine(0, y, LCD_W, color);
}

// used to fix the waveform when cursor draws over it
void restore_waveform_row(int last_y)
{
    // only fix columns that have already been drawn
    for (int x = 1; x < wave_x; x++)
    {
        int y0 = drawn_y_values[x];
        int y1 = drawn_y_values[x - 1];

        // Does the cursor cross the waveform segment between y1 to y0
        if ((y0 <= last_y && y1 >= last_y) || (y1 <= last_y && y0 >= last_y))
        {
            lcd_drawLine(x - 1, y1, x, y0, WAVEFORM_COLOR);
        }
    }
}

// ----------------- timebase stuff ----------------------------------
// -------------------------------------------------------------------

typedef enum {
    TIMEBASE_8MS,
    TIMEBASE_16MS,
    TIMEBASE_32MS,
    TIMEBASE_64MS,
    TIMEBASE_160MS,
    TIMEBASE_320MS,
    TIMEBASE_640MS,
    NUM_TIMEBASE_MODES
} timebase_mode_t;

static timebase_mode_t current_timebase = TIMEBASE_32MS;

char* timebase_str[] = {
    "8ms/div",
    "16ms/div",
    "32ms/div",
    "64ms/div",
    "160ms/div",
    "320ms/div",
    "640ms/div"
};

static float get_screen_time_window(void)
{
    switch (current_timebase) {
        case TIMEBASE_8MS:   return 0.008f;
        case TIMEBASE_16MS:  return 0.016f;
        case TIMEBASE_32MS:  return 0.032f;
        case TIMEBASE_64MS:  return 0.064f;
        case TIMEBASE_160MS: return 0.160f;
        case TIMEBASE_320MS: return 0.320f;
        case TIMEBASE_640MS: return 0.640f;
        default:             return 0.032f;
    }
}

// determine decimation factor based on timebase
static int get_decimation_factor(void)
{
    float screen_time_window = get_screen_time_window();
    float samples_per_screen = SAMPLE_RATE_HZ * screen_time_window;
    int dec = samples_per_screen / LCD_W;
    if (dec < 1) { dec = 1;} // can’t plot >1 sample per pixel
    return dec;
}

// ----------------- grid + cursor stuff -----------------------------
// -------------------------------------------------------------------

// to draw the initial grid
void lcd_draw_grid(void)
{
   lcd_fillScreen(BACKGROUND_COLOR);
   for (int i = 0; i < NUM_GRID_LINES; i++) {
       lcd_drawHLine(0, GRID_LINE_HORIZONTAL(i+1), LCD_W, BLACK);
       lcd_drawVLine(GRID_LINE_VERTICAL(i+1), 0, LCD_H, BLACK);
   }
   lcd_drawString(5, LCD_H-10, timebase_str[current_timebase], TIMEBASE_TXT_COLOR);
}

// call this every frame to erase old cursor and draw new one
void cursor_update(bool frozen, int y_curr)
{
    // only update if frozen
    if (!frozen) { return;}
    static int cursor_y = LCD_MID_HORIZONTAL;
    //adjust sensitivity
    cursor_y += (y_curr / JOY_SENSITIVITY);
    int new_x = 0; //unused for now

    // keep n_y in bounds of screen
    if (cursor_y < 0) { cursor_y = 0;}
    if (cursor_y >= LCD_H) { cursor_y = LCD_H - 1;}
    
    // no change then return
    if (cursor_y == last_y) { return;}
    
    // erase old to white
    draw_cursor(0, last_y, WHITE);
        
    // redraw horizontal grid if needed
    for (int i = 0; i < NUM_GRID_LINES; i++) {
        if (last_y == GRID_LINE_HORIZONTAL(i+1)) {
                lcd_drawHLine(0, last_y, LCD_W, BLACK); // draw horizontal grid line
            }
    }
    // redraw all vertical grid lines
    for (int j =0; j < NUM_GRID_LINES; j++) {
        lcd_drawVLine(GRID_LINE_VERTICAL(j+1), 0, LCD_H, BLACK); // draw vertical grid lines
    }
        
    // fix screen frozen text if needed
    if (frozen && ( (cursor_y < 1 || last_y < 15)  ) ) {
        lcd_drawString(5, 5, "SCREEN FROZEN", FROZEN_TXT_COLOR);
    } else if ( cursor_y > LCD_H - 10 || last_y > LCD_H - 10) {
        lcd_drawString(5, LCD_H-10, timebase_str[current_timebase], TIMEBASE_TXT_COLOR);   
    }

    // fix waveform under old cursor
    restore_waveform_row(last_y);
    // draw new cursor
    draw_cursor(new_x, cursor_y, CURSOR_COLOR);

    if (frozen) {
        float voltage = get_voltage_at_cursor(cursor_y);
        char v_txt[32];
        snprintf(v_txt, sizeof(v_txt), "%.3fV", voltage);
        lcd_fillRect(LCD_W - 62, 3, 57, 12, BLACK);
        lcd_drawString(LCD_W-60, 5, v_txt, CURSOR_COLOR);
    } else {
        lcd_fillRect(LCD_W-60,5,55,10,BLACK);
    }

    last_y = cursor_y;
}

float get_voltage_at_cursor(int cursor_y)
{
    // Find the closest drawn waveform point to the cursor
    float min_distance = LCD_H;
    int closest_x = -1;
    
    for (int x = 0; x < wave_x; x++) {
        if (drawn_y_values[x] != -1) {
            float distance = abs(drawn_y_values[x] - cursor_y);
            if (distance < min_distance) {
                min_distance = distance;
                closest_x = x;
            }
        }
    }
    
    if (closest_x == -1) {
        return 0.0f; // No valid waveform data
    }
    
    // Convert the y pixel position back to ADC value
    int y = drawn_y_values[closest_x];
    int adc_value = (y - LCD_MID_HORIZONTAL) * SLOPE_CONVERSION + ADC_MIDPOINT;
    
    // Clamp to valid ADC range
    if (adc_value < 0) adc_value = 0;
    if (adc_value >= 4096) adc_value = 4095;
    
    // Look up real voltage from LUT
    return ADC_LUT[adc_value];
}

// ----------------- waveform stuff ----------------------------------
// -------------------------------------------------------------------

// initialize waveform buffer
void waveform_display_init(void)
{
   // clear the buffer
   for (int i=0; i < SAMPLE_BUFFER_SIZE; i++) {
       sample_buffer[i] = 0;
   }
   lcd_draw_grid();
   wave_x = 0;
   last_y = -1;
}

// adds newest sample to buffer
void waveform_display_add_sample(uint16_t new_sample)
{
   sample_buffer[sample_write_index] = new_sample;
   sample_write_index = (sample_write_index+1) % SAMPLE_BUFFER_SIZE;   
}

// draw the waveform LEFT to RIGHT
void waveform_display_tick(void)
{
    float screen_time_window = get_screen_time_window();
    uint32_t samples_per_screen = (uint32_t)(SAMPLE_RATE_HZ * screen_time_window);    // reset screen and start new waveform drawing
    
    if (samples_per_screen >= SAMPLE_BUFFER_SIZE) {
        samples_per_screen = SAMPLE_BUFFER_SIZE - 1;
    }
    int dec = get_decimation_factor();
    uint32_t start_idx = (sample_write_index + SAMPLE_BUFFER_SIZE - samples_per_screen) % SAMPLE_BUFFER_SIZE;

    if (wave_x >= HW_LCD_W) {
        wave_x = 0;
        lcd_draw_grid();
        draw_cursor(0, last_y, CURSOR_COLOR);
        for (int i = 0; i < HW_LCD_W; i++) {
            drawn_y_values[i] = -1; // reset drawn y values
        }
    }

    // Pick samples corresponding to pixel
    uint32_t sample_index = (start_idx + wave_x * dec) % SAMPLE_BUFFER_SIZE;
    uint16_t adc_raw = sample_buffer[sample_index];
    int y_curr = ((adc_raw-ADC_MIDPOINT) / SLOPE_CONVERSION) + LCD_MID_HORIZONTAL;

    // save y value abt to be drawn
    drawn_y_values[wave_x] = y_curr;

    // Only draw if x > 0
    if (wave_x > 0) {
        int y_prev = drawn_y_values[wave_x - 1];
        if (y_prev != -1) {
            lcd_drawLine(wave_x - 1, y_prev, wave_x, y_curr, WAVEFORM_COLOR);
        }
    }
    wave_x++;
}

void waveform_display_draw_full_frame(void)
{
    float window = get_screen_time_window();
    uint32_t samples_per_screen = SAMPLE_RATE_HZ * window;

    if (samples_per_screen >= SAMPLE_BUFFER_SIZE) {
        samples_per_screen = SAMPLE_BUFFER_SIZE - 1;}
    int dec = samples_per_screen / LCD_W;
    if (dec < 1) { dec = 1;}
    
    uint32_t start_idx =
        (sample_write_index + SAMPLE_BUFFER_SIZE - samples_per_screen) % SAMPLE_BUFFER_SIZE;

    // Clear & redraw grid
    lcd_draw_grid();

    for (int x = 0; x < LCD_W; x++)
    {
        uint32_t sample_idx = (start_idx + x * dec) % SAMPLE_BUFFER_SIZE;
        uint16_t adc_raw = sample_buffer[sample_idx];
        int y = ((adc_raw - ADC_MIDPOINT) / SLOPE_CONVERSION) + LCD_MID_HORIZONTAL;
        drawn_y_values[x] = y;

        if (x > 0)
        {
            int y_prev = drawn_y_values[x - 1];
            lcd_drawLine(x - 1, y_prev, x, y, WAVEFORM_COLOR);
        }
    }
    wave_x = LCD_W;
    // redraw cursor
    cursor_update(false, last_y);
}

// --------------------- cycle timebase mode ---------------------------
// ---------------------------------------------------------------------

// used to cycle the timebase mode. intended use with button action
void cycle_timebase_mode(void)
{
    current_timebase = (current_timebase + 1) % NUM_TIMEBASE_MODES;
    lcd_fillScreen(BACKGROUND_COLOR);
    lcd_draw_grid();
    draw_cursor(0, last_y, CURSOR_COLOR);
    lcd_drawString(5, LCD_H - 10, timebase_str[current_timebase], TIMEBASE_TXT_COLOR);
    wave_x = 0;
}

// use to get the rate at which you should redraw the frames
int get_redraw_interval(void)
{
    switch (current_timebase) {
        case TIMEBASE_8MS:   return 2;  // can't do 1 bc of watchdog problems
        case TIMEBASE_16MS:  return 2;  // ^^ but 2 gets it to be great anyways
        case TIMEBASE_32MS:  return 2;  // ^^
        case TIMEBASE_64MS:  return 2;  // ^^ Redraw every 2 frames (66ms)
        case TIMEBASE_160MS: return 3;  // Redraw every 5 frames (165ms)
        case TIMEBASE_320MS: return 10; // Redraw every 10 frames (330ms)
        case TIMEBASE_640MS: return 20; // every 20 frames...
        default:             return 2;
    }
}


