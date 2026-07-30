#include "DavisDecode.h"

#include <math.h>

namespace DavisDecode {

uint8_t reverseBits(uint8_t value) {
  value = static_cast<uint8_t>(((value & 0xF0) >> 4) | ((value & 0x0F) << 4));
  value = static_cast<uint8_t>(((value & 0xCC) >> 2) | ((value & 0x33) << 2));
  value = static_cast<uint8_t>(((value & 0xAA) >> 1) | ((value & 0x55) << 1));
  return value;
}

uint16_t crc16Ccitt(const uint8_t* data, size_t length, uint16_t init) {
  uint16_t crc = init;
  while (length-- > 0) {
    crc ^= static_cast<uint16_t>(*data++) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000) {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
      } else {
        crc = static_cast<uint16_t>(crc << 1);
      }
    }
  }
  return crc;
}

bool validatePacket(const uint8_t* raw, uint8_t* decodedOut) {
  uint8_t decoded[kPacketLength];
  for (uint8_t i = 0; i < kPacketLength; ++i) {
    decoded[i] = reverseBits(raw[i]);
  }

  const uint16_t receivedCrc =
      (static_cast<uint16_t>(decoded[6]) << 8) | decoded[7];
  const uint16_t calculatedCrc = crc16Ccitt(decoded, 6);

  if (decodedOut != nullptr) {
    for (uint8_t i = 0; i < kPacketLength; ++i) {
      decodedOut[i] = decoded[i];
    }
  }

  return receivedCrc == calculatedCrc && calculatedCrc != 0;
}

PacketType packetType(uint8_t headerByte) {
  switch (headerByte >> 4) {
    case 0x4:
      return PacketType::Uv;
    case 0x5:
      return PacketType::RainSecs;
    case 0x6:
      return PacketType::Solar;
    case 0x8:
      return PacketType::Temp;
    case 0x9:
      return PacketType::WindGust;
    case 0xA:
      return PacketType::Humidity;
    case 0xE:
      return PacketType::Rain;
    default:
      return PacketType::Unknown;
  }
}

const char* packetTypeName(PacketType type) {
  switch (type) {
    case PacketType::Uv:
      return "uv";
    case PacketType::RainSecs:
      return "rain_secs";
    case PacketType::Solar:
      return "solar";
    case PacketType::Temp:
      return "temperature";
    case PacketType::WindGust:
      return "wind_gust";
    case PacketType::Humidity:
      return "humidity";
    case PacketType::Rain:
      return "rain";
    default:
      return "unknown";
  }
}

static uint16_t windDirectionDeg(uint8_t rawDir) {
  // ISS vane dead zone near north; raw 0 means no reading.
  if (rawDir == 0) {
    return 0;
  }
  return static_cast<uint16_t>(
      9 + static_cast<int>(roundf((rawDir - 1) * 342.0f / 255.0f)));
}

PacketFields parseFields(const uint8_t* decoded) {
  PacketFields out{};
  out.type = packetType(decoded[0]);
  out.stationId = decoded[0] & 0x07;
  out.batteryLow = (decoded[0] & 0x08) != 0;
  out.windMph = decoded[1];
  out.windDirDeg = windDirectionDeg(decoded[2]);
  out.hasPayload = false;
  out.payload = 0.0f;

  const uint8_t b3 = decoded[3];
  const uint8_t b4 = decoded[4];
  const uint16_t word34 =
      (static_cast<uint16_t>(b3) << 8) | static_cast<uint16_t>(b4);

  switch (out.type) {
    case PacketType::Temp:
      if (b3 != 0xFF) {
        // Tenths of °F packed as (b3 << 4) | (b4 >> 4)
        const int16_t tenths = static_cast<int16_t>(
            (static_cast<uint16_t>(b3) << 4) | (static_cast<uint16_t>(b4) >> 4));
        out.payload = tenths / 10.0f;
        out.hasPayload = true;
      }
      break;

    case PacketType::Humidity: {
      const uint16_t raw10 =
          (static_cast<uint16_t>(b4 >> 4) << 8) | static_cast<uint16_t>(b3);
      if (raw10 != 0) {
        out.payload = raw10 / 10.0f;
        out.hasPayload = true;
      }
      break;
    }

    case PacketType::WindGust:
      out.payload = b3;
      out.hasPayload = true;
      break;

    case PacketType::Rain:
      if (b3 != 0x80) {
        out.payload = b3;  // tip counter
        out.hasPayload = true;
      }
      break;

    case PacketType::RainSecs: {
      uint16_t secs = static_cast<uint16_t>(((b4 & 0x30) << 4) | b3);
      if (secs != 0x3FF) {
        if ((b4 & 0x40) == 0) {
          secs >>= 4;  // strong-rain encoding
        }
        out.payload = secs;
        out.hasPayload = true;
      }
      break;
    }

    case PacketType::Solar: {
      const uint16_t raw = word34 >> 6;
      if (raw < 0x3FE) {
        out.payload = raw * 1.757936f;  // W/m^2
        out.hasPayload = true;
      }
      break;
    }

    case PacketType::Uv: {
      const uint16_t raw = word34 >> 6;
      if (raw < 0x3FF) {
        out.payload = raw / 50.0f;
        out.hasPayload = true;
      }
      break;
    }

    default:
      break;
  }

  return out;
}

bool applyPacket(const uint8_t* decoded, WeatherData& weather,
                 float rainInchesPerTip) {
  const PacketFields fields = parseFields(decoded);

  weather.stationId = fields.stationId;
  weather.batteryLow = fields.batteryLow;
  weather.windMph = fields.windMph;
  if (fields.windDirDeg != 0) {
    weather.windDirDeg = fields.windDirDeg;
  }
  weather.lastType = fields.type;
  weather.lastPacketMs = millis();
  weather.valid = true;

  if (!fields.hasPayload) {
    return true;
  }

  switch (fields.type) {
    case PacketType::Temp:
      weather.temperatureF = fields.payload;
      break;
    case PacketType::Humidity:
      weather.humidityPct = fields.payload;
      break;
    case PacketType::WindGust:
      weather.windGustMph = fields.payload;
      break;
    case PacketType::Rain:
      weather.rainTips = fields.payload;
      weather.rainAccumIn = fields.payload * rainInchesPerTip;
      break;
    case PacketType::RainSecs:
      if (fields.payload > 0.0f) {
        // in/hr = (3600 / secs_between_tips) * tip_size
        weather.rainRateInHr =
            (3600.0f / fields.payload) * rainInchesPerTip;
      } else {
        weather.rainRateInHr = 0.0f;
      }
      break;
    case PacketType::Solar:
      weather.solarWm2 = fields.payload;
      break;
    case PacketType::Uv:
      weather.uvIndex = fields.payload;
      break;
    default:
      break;
  }

  return true;
}

void printFields(const PacketFields& fields) {
  Serial.print(F("  "));
  Serial.print(packetTypeName(fields.type));
  Serial.print(F(" id="));
  Serial.print(fields.stationId);
  Serial.print(F(" batt="));
  Serial.print(fields.batteryLow ? F("low") : F("ok"));
  Serial.print(F(" wind="));
  Serial.print(fields.windMph);
  Serial.print(F("mph"));
  if (fields.windDirDeg != 0) {
    Serial.print(F(" dir="));
    Serial.print(fields.windDirDeg);
    Serial.print(F("deg"));
  } else {
    Serial.print(F(" dir=n/a"));
  }

  if (fields.hasPayload) {
    Serial.print(' ');
    switch (fields.type) {
      case PacketType::Temp:
        Serial.print(F("temp="));
        Serial.print(fields.payload, 1);
        Serial.print(F("F"));
        break;
      case PacketType::Humidity:
        Serial.print(F("rh="));
        Serial.print(fields.payload, 1);
        Serial.print('%');
        break;
      case PacketType::WindGust:
        Serial.print(F("gust="));
        Serial.print(static_cast<int>(fields.payload));
        Serial.print(F("mph"));
        break;
      case PacketType::Rain:
        Serial.print(F("tips="));
        Serial.print(static_cast<int>(fields.payload));
        break;
      case PacketType::RainSecs:
        Serial.print(F("secs="));
        Serial.print(static_cast<int>(fields.payload));
        break;
      case PacketType::Solar:
        Serial.print(F("solar="));
        Serial.print(fields.payload, 0);
        Serial.print(F("W/m2"));
        break;
      case PacketType::Uv:
        Serial.print(F("uv="));
        Serial.print(fields.payload, 1);
        break;
      default:
        Serial.print(F("val="));
        Serial.print(fields.payload, 1);
        break;
    }
  }
  Serial.println();
}

}  // namespace DavisDecode
