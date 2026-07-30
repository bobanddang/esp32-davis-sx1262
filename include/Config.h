#pragma once

// Wi-Fi / MQTT credentials. Set via gitignored secrets.ini build_flags.
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef MQTT_HOST
#define MQTT_HOST "homeassistant.local"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#ifndef MQTT_USER
#define MQTT_USER ""
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

// Davis transmitter ID (0–7). Hop interval grows by 62.5 ms per ID step.
#ifndef DAVIS_STATION_ID
#define DAVIS_STATION_ID 0
#endif

// Rain tip size: 0.01" (US) or 0.2 mm (metric/EU tippers often use 0.2 mm).
#ifndef DAVIS_RAIN_INCHES_PER_TIP
#define DAVIS_RAIN_INCHES_PER_TIP 0.01f
#endif

#define MQTT_CLIENT_ID        "esp32_davis_sx1262"
#define HA_DEVICE_NAME        "Davis Weather Station"
#define HA_DEVICE_ID          "davis_vantage_pro2"
#define HA_DISCOVERY_PREFIX   "homeassistant"
#define HA_STATE_TOPIC        "homeassistant/sensor/davis_weather/state"
