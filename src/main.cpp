#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "Config.h"
#include "DavisDecode.h"
#include "DavisHopper.h"
#include "MqttManager.h"
#include "pins.h"

// FSPI (SPI2_HOST) dedicated to the Core1262-HF
SPIClass radioSPI(FSPI);

SX1262 radio = new Module(PIN_RADIO_CS, PIN_RADIO_DIO1, PIN_RADIO_RST,
                          PIN_RADIO_BUSY, radioSPI,
                          RADIOLIB_DEFAULT_SPI_SETTINGS);

DavisHopper hopper(radio);
MqttManager mqtt;

DavisDecode::WeatherData weather;
volatile bool weatherDirty = false;
int16_t lastRssi = 0;

void onDavisPacket(const uint8_t* decoded,
                   const DavisDecode::PacketFields& /*fields*/, int16_t rssi,
                   float /*snr*/, void* /*ctx*/) {
  DavisDecode::applyPacket(decoded, weather, DAVIS_RAIN_INCHES_PER_TIP);
  lastRssi = rssi;
  weatherDirty = true;
}

void setup() {
  Serial.begin(115200);
  // USB CDC can take a moment after reset; don't block forever if monitor is late.
  const uint32_t serialWaitUntil = millis() + 2000;
  while (!Serial && millis() < serialWaitUntil) {
    delay(10);
  }
  delay(200);
  Serial.println();
  Serial.println(F("esp32-davis-sx1262"));
  Serial.println(F("Davis Vantage Pro2/Vue → MQTT / Home Assistant"));
  Serial.flush();

#if defined(CONFIG_IDF_TARGET_ESP32C5)
  // Waveshare ESP32-C5-Zero RF switch: LOW = ceramic, HIGH = U.FL
  pinMode(26, OUTPUT);
  digitalWrite(26, LOW);
  Serial.println(F("[main] C5-Zero antenna=onboard (IO26 LOW)"));
#endif

  radioSPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

  hopper.setPacketCallback(onDavisPacket);
  if (!hopper.begin(DAVIS_STATION_ID)) {
    Serial.println(F("[main] radio init failed — halt"));
    while (true) {
      delay(1000);
    }
  }

  mqtt.begin();
}

void loop() {
  // Time-critical FHSS engine
  hopper.poll();

  // Wi-Fi / MQTT reconnect (non-blocking)
  mqtt.poll();

  if (weatherDirty) {
    weatherDirty = false;
    mqtt.publishState(weather, lastRssi, hopper.lastRxChannel());
  }
}
