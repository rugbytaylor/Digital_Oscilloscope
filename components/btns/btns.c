#include "btns.h"
#include "esp_timer.h"

#define DEBOUNCE_TIME_US 5000  // 5ms debounce

typedef struct {
    int64_t last_time;
} button_state_t;

static button_state_t btn_states[GPIO_NUM_MAX] = {0};

void button_init(gpio_num_t pin)
{
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);  // assumes active low
}

bool btn_pressed(gpio_num_t pin)
{
    bool raw = (gpio_get_level(pin) == 0); // active LOW
    int64_t now = esp_timer_get_time();

    if (raw && (now - btn_states[pin].last_time) > DEBOUNCE_TIME_US) {
        btn_states[pin].last_time = now;
        return true;
    }

    return false;
}