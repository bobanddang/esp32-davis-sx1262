# ESP32 Davis SX1262 Weather Bridge

Capture wireless ISS telemetry from a **Davis Vantage Pro2 / Vue** station with an ESP32 (S3/C3/C5) and **Waveshare Core1262-HF** (Semtech SX1262), then publish decoded metrics to **Home Assistant** via MQTT Auto-Discovery.

Others have already done the hard work of reverse-engineering the Davis FHSS hop tables, packet layout, and CRC — notably projects such as [DavisRFM69](https://github.com/dekay/DavisRFM69) (and related community efforts around RFM69 / SDR reception). Those implementations target different radios. This repository exists to bring that same protocol knowledge to the **SX1262 / Core1262-HF** path and wire it into Home Assistant. Huge thanks to those authors for the heavy lifting; this project stands on their shoulders.

## Hardware wiring

| Core1262-HF | ESP32-S3 | ESP32-C3 | ESP32-C5 | Notes |
|-------------|----------|----------|----------|-------|
| SCK         | 12       | 6        | 6        | FSPI / SPI2_HOST |
| MISO        | 13       | 2        | 2        | |
| MOSI        | 11       | 7        | 7        | |
| NSS (CS)    | 10       | 10       | 10       | |
| RST         | 14       | 3        | 3        | |
| BUSY        | 8        | 4        | 4        | |
| DIO1        | 9        | 5        | 5        | Packet IRQ |
| RXEN        | 3.3V     | 3.3V     | 3.3V     | Or use `setDio2AsRfSwitch(true)` |
| 3V3 / GND   | 3V3/GND  | 3V3/GND  | 3V3/GND  | |

C3/C5 share the SPI2 IO_MUX defaults (flash uses different pins than S3). On Waveshare **ESP32-C5-Zero**, GPIOs 2–7 and 10 are all on the side headers. `pins.h` selects the map from `CONFIG_IDF_TARGET_*`.

## Setup

1. Install [PlatformIO](https://platformio.org/).
2. Copy `secrets.ini.example` → `secrets.ini` and set Wi-Fi / MQTT flags (file is gitignored).
3. Build & flash (S3 default; C3/C5 envs available):
   ```bash
   pio run -e esp32-s3-devkitc1-n16r8 -t upload
   # pio run -e esp32-c3-devkitm-1 -t upload
   # pio run -e esp32-c5-devkitc1-n4 -t upload   # Waveshare ESP32-C5-Zero (4MB)
   pio device monitor
   ```

`DAVIS_STATION_ID` must match the DIP/switch ID on the ISS (0–7). Hop interval is `2562.5 ms + ID × 62.5 ms`.

## MQTT topics

| Topic | Purpose |
|-------|---------|
| `homeassistant/sensor/davis_weather/<name>/config` | HA Auto-Discovery (retained) |
| `homeassistant/sensor/davis_weather/state` | JSON telemetry |
| `homeassistant/sensor/davis_weather/status` | `online` / `offline` |

Device identifier: `davis_vantage_pro2` (single HA device “Davis Weather Station”).

### State JSON fields

`temperature_f`, `humidity_pct`, `wind_mph`, `wind_gust_mph`, `wind_dir_deg`, `rain_rate_in_hr`, `rain_tips`, `rain_accum_in`, `solar_wm2`, `uv_index`, `battery_low`, `station_id`, `rssi_dbm`, `channel`

## Architecture

| Module | Role |
|--------|------|
| `DavisHopper` | 51-channel US FHSS state machine; Lost Sync → continuous RX on ch 0 after 4 missed hops |
| `DavisDecode` | Bit-reverse, CCITT CRC-16, packet-type field extraction |
| `MqttManager` | Wi-Fi, PubSubClient, HA discovery + state publish |
| `main.cpp` | SPI bring-up, non-blocking `loop()` |

### Radio (GFSK)

- 19.2 kbps, ±9.6 kHz deviation, 156.2 kHz RX BW  
- Preamble: 4 × `0xAA`  
- Sync: `0xCB 0x89`  
- `setDio2AsRfSwitch(true)`, `setTCXO(1.6)` for Core1262-HF  

Hop frequencies use the DavisRFM69 North America FRF table (~902–928 MHz).

## Notes

- US ISS tip size defaults to **0.01 in/tip** (`DAVIS_RAIN_INCHES_PER_TIP`).
- EU 868 MHz stations need a different hop table (not enabled in this build).
- If acquisition is weak outdoors, try widening RX BW to `234.3` or `467.0` in `DavisHopper.h`.
