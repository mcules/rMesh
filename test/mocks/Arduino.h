#pragma once
// Minimal Arduino.h stand-in for native (host) unit tests.
// Only what the pure-logic modules under test actually use.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <time.h>

// Test-controllable millis(). The test defines _mock_millis and advances it.
extern unsigned long _mock_millis;
inline unsigned long millis() { return _mock_millis; }
