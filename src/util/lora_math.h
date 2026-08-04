#pragma once
#include <cstdint>

// Pure LoRa math, free of Arduino/settings/global dependencies so it can be
// unit-tested natively on the host (see test/test_toa, test/test_lora_params).

// LoRa Time-on-Air in milliseconds for a given payload and modem config.
// Returns 0 for out-of-range parameters (BW 0, SF outside 6..12) so callers
// never feed garbage into scheduling / duty-cycle accounting.
uint32_t computeToA(uint16_t payloadBytes, uint8_t sf, float bandwidthKHz,
                    uint8_t codingRate, int16_t preambleLength);

// Clamp LoRa modem parameters to the SX126x-valid ranges (SF 6..12, CR 5..8,
// preamble >= 6, bandwidth snapped to the nearest supported value). Operates in
// place on the passed references.
void clampLoraParams(uint8_t& sf, uint8_t& codingRate, float& bandwidthKHz,
                     int16_t& preambleLength);
