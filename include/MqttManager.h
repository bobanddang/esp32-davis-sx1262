#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "Config.h"
#include "DavisDecode.h"

// Wi-Fi + MQTT publisher with Home Assistant MQTT Auto-Discovery.
class MqttManager {
 public:
  MqttManager();

  bool begin();
  // Non-blocking reconnect / loop housekeeping.
  void poll();

  // Publish discovery configs (retain=true) once per boot after MQTT connect.
  void publishDiscovery();

  // Publish current weather JSON to HA_STATE_TOPIC.
  bool publishState(const DavisDecode::WeatherData& weather, int16_t rssi = 0,
                    uint8_t channel = 0);

  bool connected();

 private:
  void ensureWifi();
  void ensureMqtt();
  void publishSensorConfig(const char* objectId, const char* name,
                           const char* deviceClass, const char* unit,
                           const char* valueTemplate,
                           const char* stateClass = "measurement");

  WiFiClient wifi_;
  PubSubClient mqtt_;
  bool discoverySent_ = false;
  uint32_t lastWifiAttemptMs_ = 0;
  uint32_t lastMqttAttemptMs_ = 0;
};
