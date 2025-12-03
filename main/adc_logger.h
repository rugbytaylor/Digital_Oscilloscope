#pragma once

// Configuration for raw ADC data acquisition
// blocks until button is pressed -> logs -> button pressed again to stop
// then saves csv to SD card
void adc_logger_run( void );
