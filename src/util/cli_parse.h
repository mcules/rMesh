#pragma once
#include <stddef.h>

// Parse the argument string of "wifi add <SSID> [<PW>]".
//
// SSIDs (and passwords) containing spaces can be wrapped in double quotes:
//   MyNet secret          -> ssid "MyNet",       pw "secret"
//   "My Home Net" secret  -> ssid "My Home Net", pw "secret"
//   "My Home Net"         -> ssid "My Home Net", pw ""
//   MyNet "pw with space" -> ssid "MyNet",       pw "pw with space"
//
// Unquoted: SSID ends at the first space, everything after it is the
// password (matches the previous behaviour). An unterminated quote takes
// the rest of the line as SSID. Output buffers are always NUL-terminated;
// overlong values are truncated. Returns false if no SSID was found.
bool parseWifiAddArgs(const char* args, char* ssid, size_t ssidCap,
                      char* password, size_t pwCap);
