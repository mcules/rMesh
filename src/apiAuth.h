#pragma once

#ifdef HAS_WIFI

#include <ESPAsyncWebServer.h>

/**
 * @brief Check API authentication from the Authorization header.
 *
 * Supports two schemes:
 *  - "HMAC <timestamp>:<hex-signature>" — HMAC-SHA256 with webPasswordHash as key
 *  - "Bearer <token>" — plain token compared against webPasswordHash
 *
 * @param request  The incoming HTTP request.
 * @return true if authenticated (or no password set).
 */
bool checkApiAuth(AsyncWebServerRequest *request);

#endif
