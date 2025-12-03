#pragma once
#include <stdint.h>

// Lookup table mapping ADC readings (0-4095) to readable voltages (-5V to +5V)
extern const float ADC_LUT[4096];
