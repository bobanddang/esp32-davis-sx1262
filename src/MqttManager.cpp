#include "MqttManager.h"

#include <ArduinoJson.h>
#include <cstring>

namespace {

constexpr uint32_t kWifiRetryMs = 10000;
constexpr uint32_t kMqttRetryMs = 5000;

bool isConfigured(const char* value) {
  return value != nullptr && value[0] != '\0' &&
         strcmp(value, "YOUR_WIFI_SSID") != 0 &&
         strcmp(value, "YOUR_WIFI_PASSWORD") != 0;
}

}  // namespace

MqttManager::MqttManager() : mqtt_(wifi_) {}

bool MqttManager::begin() {
  mqtt_.setServer(MQTT_HOST, MQTT_PORT);
  mqtt_.setBufferSize(1024);
  ensureWifi();
  ensureMqtt();
  return true;
}

bool MqttManager::connected() {
  return WiFi.status() == WL_CONNECTED && mqtt_.connected();
}

void MqttManager::poll() {
  ensureWifi();
  ensureMqtt();
  if (mqtt_.connected()) {
    mqtt_.loop();
    if (!discoverySent_) {
      publishDiscovery();
      discoverySent_ = true;
    }
  }
}

void MqttManager::ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (!isConfigured(WIFI_SSID) || !isConfigured(WIFI_PASSWORD)) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastWifiAttemptMs_ < kWifiRetryMs && lastWifiAttemptMs_ != 0) {
    return;
  }
  lastWifiAttemptMs_ = now;

  Serial.print(F("[mqtt] Wi-Fi connecting to "));
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void MqttManager::ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (mqtt_.connected()) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastMqttAttemptMs_ < kMqttRetryMs && lastMqttAttemptMs_ != 0) {
    return;
  }
  lastMqttAttemptMs_ = now;

  Serial.print(F("[mqtt] connecting to "));
  Serial.println(MQTT_HOST);

  bool ok = false;
  if (MQTT_USER[0] != '\0') {
    ok = mqtt_.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
  } else {
    ok = mqtt_.connect(MQTT_CLIENT_ID);
  }

  if (ok) {
    Serial.println(F("[mqtt] connected"));
    discoverySent_ = false;
  } else {
    Serial.print(F("[mqtt] connect failed rc="));
    Serial.println(mqtt_.state());
  }
}

void MqttManager::publishSensorConfig(const char* objectId, const char* name,
                                      const char* deviceClass, const char* unit,
                                      const char* valueTemplate,
                                      const char* stateClass) {
  char topic[128];
  snprintf(topic, sizeof(topic), "%s/sensor/davis_weather/%s/config",
           HA_DISCOVERY_PREFIX, objectId);

  JsonDocument doc;
  doc["name"] = name;
  doc["unique_id"] = String("davis_") + objectId;
  doc["state_topic"] = HA_STATE_TOPIC;
  doc["value_template"] = valueTemplate;
  doc["availability_topic"] = "homeassistant/sensor/davis_weather/status";
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";

  if (deviceClass != nullptr && deviceClass[0] != '\0') {
    doc["device_class"] = deviceClass;
  }
  if (unit != nullptr && unit[0] != '\0') {
    doc["unit_of_measurement"] = unit;
  }
  if (stateClass != nullptr && stateClass[0] != '\0') {
    doc["state_class"] = stateClass;
  }

  JsonObject device = doc["device"].to<JsonObject>();
  device["identifiers"][0] = HA_DEVICE_ID;
  device["name"] = HA_DEVICE_NAME;
  device["manufacturer"] = "Davis Instruments";
  device["model"] = "Vantage Pro2 / Vue (SX1262 bridge)";
  device["sw_version"] = "1.0.0";

  char payload[768];
  const size_t n = serializeJson(doc, payload, sizeof(payload));
  mqtt_.publish(topic, reinterpret_cast<const uint8_t*>(payload), n, true);
}

void MqttManager::publishDiscovery() {
  Serial.println(F("[mqtt] publishing HA discovery"));

  mqtt_.publish("homeassistant/sensor/davis_weather/status", "online", true);

  publishSensorConfig("temperature", "Outdoor Temperature", "temperature",
                      "°F", "{{ value_json.temperature_f }}");
  publishSensorConfig("humidity", "Outdoor Humidity", "humidity", "%",
                      "{{ value_json.humidity_pct }}");
  publishSensorConfig("wind_speed", "Wind Speed", "wind_speed", "mph",
                      "{{ value_json.wind_mph }}");
  publishSensorConfig("wind_gust", "Wind Gust", "wind_speed", "mph",
                      "{{ value_json.wind_gust_mph }}");
  publishSensorConfig("wind_direction", "Wind Direction", "wind_direction",
                      "°", "{{ value_json.wind_dir_deg }}");
  publishSensorConfig("rain_rate", "Rain Rate", "precipitation_intensity",
                      "in/h", "{{ value_json.rain_rate_in_hr }}");
  publishSensorConfig("rain_tips", "Rain Tips", "", "tips",
                      "{{ value_json.rain_tips }}", "total");
  publishSensorConfig("rain_accum", "Rain Accumulation", "precipitation", "in",
                      "{{ value_json.rain_accum_in }}", "total");
  publishSensorConfig("solar", "Solar Radiation", "irradiance", "W/m²",
                      "{{ value_json.solar_wm2 }}");
  publishSensorConfig("uv", "UV Index", "", "", "{{ value_json.uv_index }}");
  publishSensorConfig("battery", "Transmitter Battery Low", "battery", "",
                      "{{ value_json.battery_low }}", "");
  publishSensorConfig("rssi", "RF RSSI", "signal_strength", "dBm",
                      "{{ value_json.rssi_dbm }}");
  publishSensorConfig("channel", "RF Channel", "", "",
                      "{{ value_json.channel }}", "");
}

bool MqttManager::publishState(const DavisDecode::WeatherData& weather,
                               int16_t rssi, uint8_t channel) {
  if (!mqtt_.connected() || !weather.valid) {
    return false;
  }

  JsonDocument doc;
  if (!isnan(weather.temperatureF)) {
    doc["temperature_f"] = round(weather.temperatureF * 10.0f) / 10.0f;
  }
  if (!isnan(weather.humidityPct)) {
    doc["humidity_pct"] = round(weather.humidityPct * 10.0f) / 10.0f;
  }
  if (!isnan(weather.windMph)) {
    doc["wind_mph"] = weather.windMph;
  }
  if (!isnan(weather.windGustMph)) {
    doc["wind_gust_mph"] = weather.windGustMph;
  }
  if (weather.windDirDeg != 0) {
    doc["wind_dir_deg"] = weather.windDirDeg;
  }
  if (!isnan(weather.rainRateInHr)) {
    doc["rain_rate_in_hr"] = round(weather.rainRateInHr * 100.0f) / 100.0f;
  }
  if (!isnan(weather.rainTips)) {
    doc["rain_tips"] = weather.rainTips;
  }
  if (!isnan(weather.rainAccumIn)) {
    doc["rain_accum_in"] = round(weather.rainAccumIn * 100.0f) / 100.0f;
  }
  if (!isnan(weather.solarWm2)) {
    doc["solar_wm2"] = round(weather.solarWm2);
  }
  if (!isnan(weather.uvIndex)) {
    doc["uv_index"] = round(weather.uvIndex * 10.0f) / 10.0f;
  }
  doc["battery_low"] = weather.batteryLow ? 1 : 0;
  doc["station_id"] = weather.stationId;
  doc["packet_type"] = DavisDecode::packetTypeName(weather.lastType);
  doc["rssi_dbm"] = rssi;
  doc["channel"] = channel;

  char payload[512];
  const size_t n = serializeJson(doc, payload, sizeof(payload));
  const bool ok =
      mqtt_.publish(HA_STATE_TOPIC, reinterpret_cast<const uint8_t*>(payload),
                    n, false);
  if (ok) {
    Serial.println(F("[mqtt] state published"));
  }
  return ok;
}
