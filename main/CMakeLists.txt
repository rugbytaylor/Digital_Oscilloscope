idf_component_register(SRCS "main.c" "waveform_display.c" 
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES joystick btns config 
                    adc_logger lcd LUT esp_adc driver
                    freertos esp_timer esp_wifi
                    )
