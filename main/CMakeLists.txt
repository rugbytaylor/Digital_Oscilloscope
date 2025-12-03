idf_component_register(
    SRCS "waveform_display.c" "main.c" "LUT.c" "adc_logger.c"
    INCLUDE_DIRS ".")

    # Only include ADC logger if enabled in sdkconfig
if(CONFIG_ENABLE_ADC_LOGGER)
    idf_component_register(
        SRCS "adc_logger.c"
        INCLUDE_DIRS "."
    )
endif()