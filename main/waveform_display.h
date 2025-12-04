#pragma once

#include <stdint.h>

void waveform_display_init(void);

void waveform_display_add_sample(uint16_t sample);

// LVGL task (keeps UI alive)
void lvgl_task(void *arg);

// Waveform update task (feeds ADC results to chart)
void waveform_update_task(void *arg);



