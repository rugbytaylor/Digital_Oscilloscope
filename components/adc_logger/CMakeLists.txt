idf_component_register(SRCS "adc_logger.c"
    INCLUDE_DIRS .
    PRIV_REQUIRES driver esp_adc esp_timer 
    freertos esp_wifi fatfs  esp_driver_gptimer
    )
