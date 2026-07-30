#pragma once

#include <Arduino.h>
#include <stdint.h>

// Davis Vantage Pro2 / Vue ISS packet validation and field extraction.
// Packets are 8 bytes: 6 payload + 2-byte CCITT CRC-16 (after bit-reversal).

namespace DavisDecode {

constexpr uint8_t kPacketLength = 8;
constexpr uint8_t kSyncByte0 = 0xCB;
constexpr uint8_t kSyncByte1 = 0x89;

enum class PacketType : uint8_t {
  Uv = 0x4,
  RainSecs = 0x5,
  Solar = 0x6,
  Temp = 0x8,
  WindGust = 0x9,
  Humidity = 0xA,
  Rain = 0xE,
  Unknown = 0xFF,
};

// Latest decoded sensor snapshot (updated as packets arrive).
struct WeatherData {
  float temperatureF = NAN;
  float humidityPct = NAN;
  float windMph = NAN;
  float windGustMph = NAN;
  uint16_t windDirDeg = 0;  // 0 = no vane reading
  float rainRateInHr = NAN;
  float rainTips = NAN;     // bucket tip counter (raw)
  float rainAccumIn = NAN;  // tips * tip size
  float solarWm2 = NAN;
  float uvIndex = NAN;
  bool batteryLow = false;
  uint8_t stationId = 0;
  PacketType lastType = PacketType::Unknown;
  uint32_t lastPacketMs = 0;
  bool valid = false;
};

struct PacketFields {
  PacketType type;
  uint8_t stationId;
  bool batteryLow;
  uint8_t windMph;
  uint16_t windDirDeg;
  bool hasPayload;
  float payload;
};

uint8_t reverseBits(uint8_t value);
uint16_t crc16Ccitt(const uint8_t* data, size_t length, uint16_t init = 0);

// Bit-reverse raw radio bytes, validate CRC; optionally copy decoded bytes.
bool validatePacket(const uint8_t* raw, uint8_t* decodedOut = nullptr);

PacketType packetType(uint8_t headerByte);
const char* packetTypeName(PacketType type);
PacketFields parseFields(const uint8_t* decoded);

// Merge a validated packet into the running weather snapshot.
bool applyPacket(const uint8_t* decoded, WeatherData& weather,
                 float rainInchesPerTip = 0.01f);

void printFields(const PacketFields& fields);

}  // namespace DavisDecode
