# Hardware Component Modules

This folder contains modular components used throughout the project. Each folder provides a focused piece of hardware or UI functionality.

## Folder Overview

Folder: adc_logger  
Purpose: ADC sampling, buffering, and basic data collection.

Folder: btns  
Purpose: Simple button handling and debounce helpers.

Folder: config  
Purpose: Shared configuration headers and project constants. Lots of GPIO pin mapping within `config.h`

Folder: joystick  
Purpose: Joystick input handling, scaling, and direction mapping.

Folder: lcd  
Purpose: Display driver and drawing utilities. Modified from esp-idf-st7789 library

Folder: LUT  
Purpose: Lookup tables for fast value conversions. Custom make these with the adc_logger files

