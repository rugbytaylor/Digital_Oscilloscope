#pragma once
#include "driver/gpio.h"
#include <stdbool.h>

// configure btn pins
void button_init(gpio_num_t pin);

// return 1 if button is pressed else return 0
bool btn_pressed(gpio_num_t pin);
