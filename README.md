![CI](https://github.com/rugbytaylor/Digital_Oscilloscope/actions/workflows/build.yaml/badge.svg)

# Digital_Oscilloscope
This project implements a **low-cost digital oscilloscope** using an ESP32, ADC sampling,
real-time waveform display, and efficient embedded C design.  
This repo is fully integrated with a **modern DevOps pipeline** using GitHub Actions.

## Project Features
- Real-time ADC data capture @ 10kHz
- Captures inputs from +5V to -4.5V with +-25mV accuracy
- Waveform rendering on MSP240x LCD screen
- Button-controlled triggering
- Efficient use of ESP32 timers & DMA
- Future support for **OTA firmware updates**

## DevOps Pipeline
Every push to `main` triggers:

| Stage | Description |
|-------|-------------|
| **CI Build** | Compiles firmware using a clean ESP-IDF toolchain |
| **Artifact Export** | Uploads `.bin` and flashing metadata automatically |
| **Automated Release** | Each push generates a versioned firmware release |
| **Scalable for OTA** | Easily upgradeable to push OTA-ready firmware |

✔ No “works on my machine” issues  
✔ Always have a downloadable firmware version  
✔ Ready for remote deployment (OTA)

---

## Firmware Downloads
Download the **latest firmware** here:

👉 https://github.com/rugbytaylor/Digital_Oscilloscope/releases/latest

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

---

## License
MIT — free to modify and improve! Pull Requests welcome.

---

## Future Improvements
- Enable OTA update (using GitHub release binaries)
- Automatic versioning
- Static code checks (lint + safety)
- Hardware trigger and oversampling
- Higher-rate ADC streaming