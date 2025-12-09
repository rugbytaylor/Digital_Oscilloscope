#pragma once

#include <stdint.h>

typedef struct {
    int x;
    int y;
} joystick_pos_t;

// to initialize and setup the joystick
void joystick_init(void);

// to read the current joystick position
void joystick_read(joystick_pos_t *pos);
