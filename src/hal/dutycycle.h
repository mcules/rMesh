#pragma once

// Duty cycle monitoring for the public band (869.4–869.65 MHz, 10% in 60 s)
// Only active for isPublicBand(); amateur radio nodes are exempt.

void dutyCycleTrackTx(uint32_t toaMs);
bool dutyCycleAllowed(uint32_t toaMs);
