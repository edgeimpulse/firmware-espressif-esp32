# Edge Impulse firmware for Espressif ESP32

Edge Impulse enables developers to create the next generation of intelligent device solutions with embedded Machine Learning. This repository contains the Edge Impulse firmware for the Espressif ESP32 based development boards, specifically ESP-EYE (ESP32) and FireBeetle Board (ESP32). These devices support Edge Impulse device features, including ingestion and inferencing.

**Note: Do you just want to use this development board with Edge Impulse? No need to build this firmware. See the instructions [here](https://docs.edgeimpulse.com/docs/espressif-esp32) for a prebuilt firmware and instructions. Or, you can use the [data forwarder](https://docs.edgeimpulse.com/docs/cli-data-forwarder) to capture data from any sensor.**

## Requirements

### Hardware

- Espressif ESP32 based development boards, preferably ESP-EYE (ESP32) and FireBeetle Board (ESP32). Using with other boards is possible, but code modifications is needed. For more on that read **Using with other ESP32 boards**.

### Tools
Install ESP IDF v5.1.1, following the instructions for your OS from [this page](https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32/get-started/index.html#installation-step-by-step). You need this exact version - future versions might work, but not tested.

### Building the application
Then from the firmware folder execute:
```bash
get_idf
clear && idf.py build
```
```get_idf``` is an alias for export.sh script that sets up ESP IDF environment variables. Read more about it [here](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/get-started/index.html#step-4-set-up-the-environment-variables).

### Flash

Connect the ESP32 board to your computer.

Run:
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

Where ```/dev/ttyUSB0``` needs to be changed to actual port where ESP32 is connected on your system.

### Serial connection

Use screen, minicom or Serial monitor in Arduino IDE to set up a serial connection over USB. The following UART settings are used: 115200 baud, 8N1.

### Using with other ESP32 boards

ESP32 is a very popular chip both in a community projects and in industry, due to its high performance, low price and large amount of documentation/support available. There are other camera enabled development boards based on ESP32, which can use Edge Impulse firmware after applying certain changes, e.g.

- AI-Thinker ESP-CAM
- M5STACK ESP32 PSRAM Timer Camera X (OV3660)
- M5STACK ESP32 Camera Module Development Board (OV2640)

The pins used for camera connection on different development boards are not the same, therefore you will need to change the #define [here](https://github.com/edgeimpulse/firmware-espressif-esp32/blob/main/edge-impulse/ingestion-sdk-platform/sensors/ei_camera.h#L29) to fit your development board, compile and flash the firmware. Specifically for AI-Thinker ESP-CAM, since this board needs an external USB to TTL Serial Cable to upload the code/communicate with the board, the data transfer baud rate must be changed to 115200 [here](https://github.com/edgeimpulse/firmware-espressif-esp32/blob/main/edge-impulse/ingestion-sdk-platform/espressif_esp32/ei_device_espressif_esp32.h#35).

The analog sensor and LIS3DH accelerometer can be used on any other development board without changes, as long as the interface pins are not changed. If I2C/ADC pins that accelerometer/analog sensor are connected to are different, from described in Sensors available section, you will need to [change the values](https://github.com/AIWintermuteAI/LIS3DHTR_ESP-IDF/blob/641bda8c3e4b706a2365fe87dd4d925f96ea3f8c/src/include/LIS3DHTR.h#L31) in LIS3DHTR component for ESP32, compile and flash it to your board.

Additionally, since Edge Impulse firmware is open-source and available to public, if you have made modifications/added new sensors capabilities, we encourage you to make a PR in firmware repository!
