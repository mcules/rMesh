#include "util/lora_math.h"
#include <cmath>

uint32_t computeToA(uint16_t payloadBytes, uint8_t sf, float bandwidthKHz,
                    uint8_t codingRate, int16_t preambleLength) {
    uint32_t BW = (uint32_t)(bandwidthKHz * 1000);
    uint8_t  CR = (codingRate > 4) ? (uint8_t)(codingRate - 4) : codingRate;
    // Guard: an out-of-range SF would make (1<<SF) overflow or bitsPerSymbol 0
    // (divide-by-inf -> UB); BW 0 divides by zero.
    if (BW == 0 || sf < 6 || sf > 12) return 0;
    bool  DE           = (((1 << sf) * 1000 / BW) > 16);
    float Tsym         = (float)(1 << sf) / (float)BW * 1000.0f;
    float Tpreamble    = (preambleLength + 4.25f) * Tsym;
    float payloadBits  = 8.0f * payloadBytes - 4.0f * sf + 28.0f + 16.0f;  // +16 CRC
    float bitsPerSymbol = 4.0f * (sf - (DE ? 2 : 0));
    float payloadSymbols = 8.0f + fmaxf(ceilf(payloadBits / bitsPerSymbol) * (CR + 4), 0.0f);
    return (uint32_t)roundf(Tpreamble + (payloadSymbols * Tsym));
}

void clampLoraParams(uint8_t& sf, uint8_t& codingRate, float& bandwidthKHz,
                     int16_t& preambleLength) {
    if (sf < 6)  sf = 6;
    if (sf > 12) sf = 12;
    if (codingRate < 5) codingRate = 5;   // 4/5 .. 4/8
    if (codingRate > 8) codingRate = 8;
    if (preambleLength < 6) preambleLength = 6;
    static const float bwValid[] = {7.8f, 10.4f, 15.6f, 20.8f, 31.25f,
                                    41.7f, 62.5f, 125.0f, 250.0f, 500.0f};
    bool ok = false;
    for (float b : bwValid) { if (fabsf(bandwidthKHz - b) < 0.01f) { ok = true; break; } }
    if (!ok) bandwidthKHz = 125.0f;
}
