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
#elif defined(HELTEC_HT_TRACKER_V1_2)
    #include "hal_HELTEC_HT-Tracker_V1_2.h"
#elif defined(HELTEC_WIFI_LORA_32_V3)
    #include "hal_HELTEC_WiFi_LoRa_32_V3.h"
#elif defined(LILYGO_T_LORA_PAGER)
    #include "hal_LILYGO_T-LoraPager.h"
#elif defined(SEEED_SENSECAP_INDICATOR)
    #include "hal_SEEED_SenseCAP_Indicator.h"
#elif defined(ESP32_WITHOUT_LORA)
    #include "hal_ESP32_Without_LoRa.h"
#elif defined(ESP32_E22_V1)
    #include "hal_ESP32_E22_V1.h"
#elif defined(SEEED_XIAO_ESP32S3_WIO_SX1262)
    #include "hal_SEEED_XIAO_ESP32S3_Wio_SX1262.h"
#elif defined(LILYGO_T_ETH_ELITE_SX1262)
    #include "hal_LILYGO_T-ETH-Elite_SX1262.h"
#elif defined(LILYGO_T_ECHO)
    #include "hal_LILYGO_T-Echo.h"
#else
    #error "No HAL defined for this board!"
#endif


