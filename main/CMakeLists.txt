idf_component_register(
    SRCS "waveform_display.c" "main.c" "LUT.c" "adc_logger.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_timer driver esp_adc freertos esp_wifi fatfs
)
