#pragma once

#include <Arduino.h>
#include <RadioLib.h>

#include "DavisDecode.h"

// Frequency-hopping state machine for Davis Vantage ISS (US 915 MHz band).
// Tracks the 51-channel hop table with ~2.5625 s intervals. After
// kMaxMissedHops consecutive misses, returns to continuous listen on ch 0.

class DavisHopper {
 public:
  enum class State : uint8_t {
    LostSync,   // Continuous RX on channel 0 until a valid packet
    Tracking,   // Locked; hop on the ISS interval timer
  };

  // US hop interval base (station ID 0). Each ID adds +62.5 ms.
  static constexpr uint32_t kHopIntervalBaseMs = 2562;
  static constexpr uint32_t kHopIntervalIdStepMs = 63;  // ≈ 62.5 ms
  static constexpr uint8_t kMaxMissedHops = 4;
  static constexpr uint8_t kHopCount = 51;
  static constexpr uint8_t kPayloadLen = DavisDecode::kPacketLength;

  // GFSK modem parameters matching Davis ISS / Core1262-HF.
  static constexpr float kBitRateKbps = 19.2f;
  static constexpr float kFreqDevKhz = 9.6f;
  static constexpr float kRxBandwidthKhz = 156.2f;
  static constexpr uint16_t kPreambleBits = 32;  // 4 × 0xAA
  static constexpr float kTcxoVolts = 1.6f;

  using PacketCallback =
      void (*)(const uint8_t* decoded, const DavisDecode::PacketFields& fields,
               int16_t rssi, float snr, void* ctx);

  DavisHopper(SX1262& radio);

  // Configure modem, DIO2 RF switch, TCXO; start LostSync on channel 0.
  bool begin(uint8_t stationId = 0);

  void setStationId(uint8_t stationId);
  void setPacketCallback(PacketCallback cb, void* ctx = nullptr);

  // Non-blocking; call frequently from loop() or a FreeRTOS task.
  void poll();

  State state() const { return state_; }
  uint8_t channel() const { return hopIndex_; }
  uint8_t lastRxChannel() const { return lastRxChannel_; }
  uint32_t packetsOk() const { return packetsOk_; }
  uint32_t packetsBadCrc() const { return packetsBadCrc_; }
  uint32_t missedHops() const { return missedHopsTotal_; }

  // Hop frequencies in hop-order (not sorted by MHz). From DavisRFM69 US FRF.
  static const float kHopFrequenciesMHz[kHopCount];

 private:
  bool configureModem();
  bool tuneChannel(uint8_t index, bool startRx);
  bool startReceive();
  void enterLostSync();
  void hopNext();
  void handleRxDone();
  void onValidPacket();
  void onMissedHop();
  uint32_t hopIntervalMs() const;

  SX1262& radio_;

  State state_ = State::LostSync;
  uint8_t hopIndex_ = 0;
  uint8_t lastRxChannel_ = 0;
  uint8_t stationId_ = 0;
  uint8_t missStreak_ = 0;

  uint32_t lastRxMs_ = 0;
  uint32_t hopDueMs_ = 0;

  uint32_t packetsOk_ = 0;
  uint32_t packetsBadCrc_ = 0;
  uint32_t missedHopsTotal_ = 0;

  volatile bool irqFlag_ = false;
  PacketCallback callback_ = nullptr;
  void* callbackCtx_ = nullptr;

  static DavisHopper* instance_;
  static void IRAM_ATTR onDio1Isr();
};
