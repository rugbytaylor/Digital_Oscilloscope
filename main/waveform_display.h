#pragma once

#include <stdint.h>

// clears and initializes the sample buffer, resets drawing state, and renders initial grid
void waveform_display_init(void);

// adds newest adc_raw sample to circular buffer for real voltage extraction later
void waveform_display_add_sample(uint16_t sample);

// live waveform renderer for running mode. draws one new column of waveform data each call (no full-screen redraw)
void waveform_display_tick(void);

//updates horizontal cursor position and draws it with voltage readout. only works when screen is frozen
void cursor_update(bool frozen, int y_curr);

// full waveform redraw based on current ADC buffer and timebase
void waveform_display_draw_full_frame(void);

// rescales waveform horizontally and fully refreshes the display
void cycle_timebase_mode(void);

// used to get the frame rate at which to redraw the waveform based on the timebase mode
int get_redraw_interval(void);

// translates the cursor position to a real voltage level using the ADC_LUT from LUT.c
float get_voltage_at_cursor(int cursor_y);
