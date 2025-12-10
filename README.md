![CI](https://github.com/rugbytaylor/Digital_Oscilloscope/actions/workflows/build.yaml/badge.svg)

# Digital_Oscilloscope
This project implements a **low-cost digital oscilloscope** using an ESP32, ADC sampling,
real-time waveform display, and efficient embedded C design.  
This repo is fully integrated with a **modern DevOps pipeline** using GitHub Actions.

## Project Features
- Real-time ADC data capture @ 10kHz
- Captures inputs from +5V to -5V with +-25mV accuracy
- Waveform rendering on MSP240x LCD screen
- Button-controlled triggering
- Joystick-controlled cursor with real voltage readings
- Efficient use of ESP32 timers & DMA
- Future support for **OTA firmware updates**

## DevOps Pipeline

All development work should be pushed to the "off" branch. The pipeline runs automatically and produces builds and releases without manual steps from the developer.

How to interact with the pipeline:
1. Commit changes to the off branch.
2. Push to the remote repository. This triggers the CI workflow.
3. Review the build results on the Actions tab.
4. Download firmware artifacts or use the generated release if testing new images.

| Stage | Description |
|-------|-------------|
| **CI Build** | Compiles firmware in a clean ESP-IDF environment and applies automatic clang-format rules. |
| **Artifact Export** | Publishes the compiled bin files and associated flashing metadata. |
| **Automated Release** | Creates a versioned firmware release for every successful push. |
| **OTA Ready Output** | Produces firmware images suitable for an OTA deployment workflow. |

---

## Firmware Downloads
Download the **latest firmware** here:
https://github.com/rugbytaylor/Digital_Oscilloscope/releases/latest

Use `esptool.py` or ESP-IDF to flash `.bin` files to your ESP32.

---

## OTA Update Support (Planned)
The automated release files can be served directly to OTA update code.

OTA workflow eventually will be:
ESP32 startup →
Checks GitHub release version →
Downloads new firmware (.bin) →
Flashes to OTA partition →
Reboots

This means you’ll never need a USB cable again to deploy updates!

---

## Requirements
- ESP-IDF v5.0+ installed locally
- ESP32 development board
- MSP240x display
- Joystick
- Buttons

---

## License
MIT — free to modify and improve! Pull Requests welcome.

---

## Future Improvements
- Enable OTA update (using GitHub release binaries)
- Unit tests for code
- Hardware trigger and oversampling
- Higher-rate ADC streaming