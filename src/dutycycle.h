#pragma once

// Duty-Cycle-Überwachung für das Public-Band (869,4–869,65 MHz, 10 % in 60 s)
// Wird nur für isPublicBand() aktiv; Amateurfunk-Nodes sind davon ausgenommen.

void dutyCycleTrackTx(uint32_t toaMs);
bool dutyCycleAllowed(uint32_t toaMs);
