#pragma once

// HAL-Dispatch 

#if defined(LILYGO_T3_LORA32_V1_6_1)
    #include "hal_LILYGO_T3_LoRa32_V1_6_1.h"
#elif defined(LILYGO_T_BEAM)
    #include "hal_LILYGO_T-Beam.h"
#elif defined(HELTEC_WIRELESS_STICK_LITE_V3)
    #include "hal_HELTEC_Wireless_Stick_Lite_V3.h"
#elif defined(HELTEC_WIFI_LORA_32_V4)
    #include "hal_HELTEC_WiFi_LoRa_32_V4.h"
#elif defined(HELTEC_WIFI_LORA_32_V3)
    #include "hal_HELTEC_WiFi_LoRa_32_V3.h"
#elif defined(ESP32_WITHOUT_LORA)
    #include "hal_ESP32_Without_LoRa.h"
#else
    #error "No HAL defined for this board!"
#endif


