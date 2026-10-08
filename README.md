# ESP32 Air Quality Monitoring

An ESP32-based IoT prototype that monitors temperature, humidity, particulate matter, and a relative MQ135 gas-sensor indicator. Readings are displayed on an OLED, logged to a microSD card, and sent to Blynk IoT.

## Features

- DHT22 temperature and humidity readings
- PMS5003 PM2.5 and PM10 particulate readings
- MQ135 gas sensor reading with temperature/humidity compensation when DHT22 data is available
- SH1106 128 × 64 OLED display and DS1307 real-time clock
- CSV logging to microSD (`/data.csv`)
- Blynk IoT dashboard values on virtual pins V0–V4
- Sensor updates every 5 seconds and OLED updates every 1 second using `millis()`
- Wi-Fi connection timeout and an offline startup path

## Hardware

| Component | Purpose | Connections used in sketch |
| --- | --- | --- |
| ESP32 | Controller | — |
| DHT22 | Temperature/humidity | GPIO 13 |
| MQ135 | Relative gas-sensor indicator | GPIO 34 (ADC) |
| PMS5003 | Particulate matter | UART2: RX GPIO 16, TX GPIO 17 |
| SH1106 OLED (I²C) | Local display | ESP32 default I²C pins, per board configuration |
| DS1307 RTC (I²C) | Date and time | ESP32 default I²C pins, per board configuration |
| microSD module | CSV storage | CS GPIO 5 (other SPI pins use board defaults) |

Check your module voltage and wiring requirements before connecting hardware.

## Setup with Arduino IDE

1. Install ESP32 board support in Arduino IDE and choose your ESP32 board.
2. Install the required libraries: **Blynk**, **U8g2**, **DHT sensor library** (and **Adafruit Unified Sensor**), **RTClib**, **MQ135**, and **Adafruit PM25 AQI**. `WiFi`, `Wire`, `SPI`, and `SD` come from the ESP32/Arduino environment. The MQ135 library variant must support `getPPM()` and `getCorrectedPPM()`.
3. Make a local copy of `config.example.h` named `config.h` **in the same folder as the `.ino` file**.
4. In `config.h`, enter your own Blynk template ID, template name, auth token, and Wi-Fi SSID/password. Do **not** commit `config.h` to GitHub.
5. Open `ESP32-Air-Quality-Monitoring.ino` in Arduino IDE, select the correct board and port, then verify and upload.
6. In Blynk, configure datastreams for V0 (temperature), V1 (humidity), V2 (gas indicator), V3 (PM2.5), and V4 (PM10).

## Notes and limitations

- **The value called `VOCs` in the sketch is not a calibrated VOC concentration.** The algorithm applies a changing baseline and a random component to the MQ135 reading, so it should only be treated as a demonstration/relative indicator. Do not use it for health or safety decisions.
- The OLED air-quality labels are demonstration thresholds, not an official AQI calculation.
- The sketch attempts Blynk uploads from the periodic task; depending on connectivity, cloud writes may fail. Hardware compatibility and full behavior have not been verified in this release.
- The sketch appends a CSV header on successful SD initialization, even when a file already exists. It is retained from the original sketch.

## Security

The tracked project uses `config.example.h` with placeholders. Real network credentials and Blynk tokens belong only in the local, gitignored `config.h`. If credentials from an earlier version were exposed or shared, rotate the affected Wi-Fi password and Blynk token.
