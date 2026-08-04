#pragma once
// Native unit-test stub. frame.cpp #includes ArduinoJson but only uses it inside
// HAS_WIFI-guarded code (monitorJSON), which is compiled out in the native env
// (-UHAS_WIFI). No ArduinoJson symbols are referenced, so this can stay empty.
