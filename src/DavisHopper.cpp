#include "DavisHopper.h"

// Hop ORDER from DavisRFM69 US FRF table (not sorted by frequency).
// f_MHz = FRF_24bit * 32e6 / 2^19 / 1e6
const float DavisHopper::kHopFrequenciesMHz[DavisHopper::kHopCount] = {
    911.413818f, 902.381897f, 911.915161f, 922.953186f, 914.926575f,
    906.395874f, 925.964600f, 918.438354f, 908.904663f, 920.445374f,
    913.420410f, 903.888062f, 916.933533f, 924.458923f, 910.409912f,
    904.890625f, 915.929138f, 921.448364f, 907.399414f, 926.967651f,
    912.919067f, 903.385376f, 917.434387f, 923.456299f, 909.406860f,
    926.466370f, 905.894592f, 914.424316f, 919.441406f, 924.960205f,
    902.884521f, 910.912109f, 921.949707f, 915.427856f, 906.898071f,
    917.935669f, 927.469849f, 920.947083f, 908.402893f, 912.417358f,
    918.940125f, 904.389343f, 923.957153f, 916.431824f, 909.909058f,
    919.943604f, 905.391968f, 922.451904f, 907.901123f, 913.922607f,
    925.462402f,
};

DavisHopper* DavisHopper::instance_ = nullptr;

DavisHopper::DavisHopper(SX1262& radio) : radio_(radio) {}

void IRAM_ATTR DavisHopper::onDio1Isr() {
  if (instance_ != nullptr) {
    instance_->irqFlag_ = true;
  }
}

void DavisHopper::setStationId(uint8_t stationId) {
  stationId_ = stationId & 0x07;
}

void DavisHopper::setPacketCallback(PacketCallback cb, void* ctx) {
  callback_ = cb;
  callbackCtx_ = ctx;
}

uint32_t DavisHopper::hopIntervalMs() const {
  return kHopIntervalBaseMs +
         (static_cast<uint32_t>(stationId_) * kHopIntervalIdStepMs);
}

bool DavisHopper::begin(uint8_t stationId) {
  instance_ = this;
  setStationId(stationId);

  const float startMhz = kHopFrequenciesMHz[0];
  int16_t state = radio_.beginFSK(startMhz, kBitRateKbps, kFreqDevKhz,
                                  kRxBandwidthKhz, 10, kPreambleBits,
                                  kTcxoVolts);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[hopper] beginFSK failed: "));
    Serial.println(state);
    return false;
  }

  // Core1262-HF: RF switch on DIO2, TCXO on DIO3 at 1.6 V.
  state = radio_.setDio2AsRfSwitch(true);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[hopper] setDio2AsRfSwitch failed: "));
    Serial.println(state);
    return false;
  }

  state = radio_.setTCXO(kTcxoVolts);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[hopper] setTCXO failed: "));
    Serial.println(state);
    return false;
  }

  if (!configureModem()) {
    return false;
  }

  radio_.setPacketReceivedAction(onDio1Isr);
  enterLostSync();
  Serial.println(F("[hopper] ready — listening channel 0"));
  return true;
}

bool DavisHopper::configureModem() {
  // Preamble bytes 0xAA appear as leading sync symbols for acquisition.
  uint8_t sync[] = {0xAA, 0xAA, DavisDecode::kSyncByte0,
                    DavisDecode::kSyncByte1};

  int16_t state = radio_.setFrequencyDeviation(kFreqDevKhz);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[hopper] setFrequencyDeviation failed: "));
    Serial.println(state);
    return false;
  }

  state = radio_.setPreambleLength(kPreambleBits);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[hopper] setPreambleLength failed: "));
    Serial.println(state);
    return false;
  }

  state = radio_.fixedPacketLengthMode(kPayloadLen);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[hopper] fixedPacketLengthMode failed: "));
    Serial.println(state);
    return false;
  }

  state = radio_.setSyncWord(sync, sizeof(sync));
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[hopper] setSyncWord failed: "));
    Serial.println(state);
    return false;
  }

  radio_.setWhitening(false);
  radio_.setDataShaping(RADIOLIB_SHAPING_0_5);
  radio_.setCRC(0);  // Davis uses its own CRC-16 in the payload
  radio_.setRxBoostedGainMode(true);

  Serial.println(F("[hopper] GFSK 19.2 kbps / fdev 9.6 / bw 156.2 / sync AA AA CB 89"));
  return true;
}

bool DavisHopper::startReceive() {
  return radio_.startReceive() == RADIOLIB_ERR_NONE;
}

bool DavisHopper::tuneChannel(uint8_t index, bool startRx) {
  radio_.standby();
  const float mhz = kHopFrequenciesMHz[index];
  const int16_t state = radio_.setFrequency(mhz);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[hopper] setFrequency failed: "));
    Serial.println(state);
    return false;
  }
  hopIndex_ = index;
  irqFlag_ = false;
  if (startRx) {
    return startReceive();
  }
  return true;
}

void DavisHopper::enterLostSync() {
  state_ = State::LostSync;
  missStreak_ = 0;
  Serial.println(F("[hopper] LOST SYNC — continuous RX on channel 0"));
  tuneChannel(0, true);
}

void DavisHopper::hopNext() {
  const uint8_t next =
      static_cast<uint8_t>((hopIndex_ + 1) % kHopCount);
  tuneChannel(next, true);
  hopDueMs_ = millis() + hopIntervalMs();
}

void DavisHopper::onValidPacket() {
  lastRxMs_ = millis();
  lastRxChannel_ = hopIndex_;
  missStreak_ = 0;
  packetsOk_++;

  if (state_ != State::Tracking) {
    state_ = State::Tracking;
    Serial.print(F("[hopper] LOCKED ch="));
    Serial.println(hopIndex_);
  }

  // Advance immediately after a good packet (ISS has already hopped).
  hopNext();
}

void DavisHopper::onMissedHop() {
  missedHopsTotal_++;
  missStreak_++;
  Serial.print(F("[hopper] miss streak="));
  Serial.print(missStreak_);
  Serial.print(F(" ch="));
  Serial.println(hopIndex_);

  if (missStreak_ >= kMaxMissedHops) {
    enterLostSync();
    return;
  }
  hopNext();
}

void DavisHopper::handleRxDone() {
  uint8_t raw[kPayloadLen] = {};
  const int16_t state = radio_.readData(raw, kPayloadLen);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[hopper] readData failed: "));
    Serial.println(state);
    startReceive();
    return;
  }

  uint8_t decoded[kPayloadLen] = {};
  if (!DavisDecode::validatePacket(raw, decoded)) {
    packetsBadCrc_++;
    Serial.println(F("[hopper] CRC fail"));
    // Still treat as a hop event when tracking — timing is driven by TX.
    if (state_ == State::Tracking) {
      onMissedHop();
    } else {
      startReceive();
    }
    return;
  }

  const DavisDecode::PacketFields fields = DavisDecode::parseFields(decoded);

  // Drop packets from other transmitters (ID also sets hop interval).
  if (fields.stationId != stationId_) {
    Serial.print(F("[hopper] ignore station id="));
    Serial.println(fields.stationId);
    startReceive();
    return;
  }

  const int16_t rssi = radio_.getRSSI(false);
  const float snr = radio_.getSNR();
  lastRxChannel_ = hopIndex_;

  DavisDecode::printFields(fields);
  if (callback_ != nullptr) {
    callback_(decoded, fields, rssi, snr, callbackCtx_);
  }

  onValidPacket();
}

void DavisHopper::poll() {
  if (irqFlag_) {
    irqFlag_ = false;
    handleRxDone();
    return;
  }

  if (state_ != State::Tracking) {
    return;
  }

  const uint32_t now = millis();
  // Hop window: expect packet near hopDueMs_; allow small slack.
  constexpr uint32_t kSlackMs = 400;
  if (static_cast<int32_t>(now - hopDueMs_) > static_cast<int32_t>(kSlackMs)) {
    onMissedHop();
  }
}
