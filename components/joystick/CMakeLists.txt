idf_component_register(SRCS "joystick_dma.c"
                        INCLUDE_DIRS .
                        PRIV_REQUIRES driver esp_adc
                        freertos config
                        )