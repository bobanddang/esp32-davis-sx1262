#pragma once

// Waveshare Core1262-HF SPI + control pins.
// C3/C5: use SPI2 IO_MUX defaults (flash occupies other GPIOs than on S3).

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5)
// ESP32-C3 / ESP32-C5 — SPI2 dedicated pins (also fit Waveshare C5-Zero headers)
#define PIN_RADIO_CS    10
#define PIN_RADIO_DIO1  5
#define PIN_RADIO_RST   3
#define PIN_RADIO_BUSY  4

#define PIN_SPI_MOSI    7
#define PIN_SPI_MISO    2
#define PIN_SPI_SCK     6
#else
// ESP32-S3 DevKit (Lonely Binary / similar) — FSPI / SPI2_HOST
#define PIN_RADIO_CS    10
#define PIN_RADIO_DIO1  9
#define PIN_RADIO_RST   14
#define PIN_RADIO_BUSY  8

#define PIN_SPI_MOSI    11
#define PIN_SPI_MISO    13
#define PIN_SPI_SCK     12
#endif

// RXEN tied to 3.3V (or driven by SX1262 DIO2 RF-switch command)
