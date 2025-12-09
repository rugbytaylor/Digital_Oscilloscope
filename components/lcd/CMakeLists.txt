idf_component_register(SRCS lcd.c
                       INCLUDE_DIRS .
                       PRIV_REQUIRES driver
                       REQUIRES config esp_driver_spi 
                       esp_driver_gpio freertos)
# target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
