#pragma once

// HAL-Dispatch 

#if defined(LILYGO_T3_LORA32_V1_6_1)
    #include "bsp/boards/LILYGO_T3_LoRa32_V1_6_1/hal_LILYGO_T3_LoRa32_V1_6_1.h"
#elif defined(LILYGO_T_BEAM)
    #include "bsp/boards/LILYGO_T_Beam/hal_LILYGO_T_Beam.h"
#elif defined(HELTEC_WIRELESS_STICK_LITE_V3)
    #include "bsp/boards/HELTEC_Wireless_Stick_Lite_V3/hal_HELTEC_Wireless_Stick_Lite_V3.h"
#elif defined(HELTEC_WIFI_LORA_32_V4)
    #include "bsp/boards/HELTEC_WiFi_LoRa_32_V4/hal_HELTEC_WiFi_LoRa_32_V4.h"
#elif defined(HELTEC_HT_TRACKER_V1_2)
    #include "bsp/boards/HELTEC_HT_Tracker_V1_2/hal_HELTEC_HT_Tracker_V1_2.h"
#elif defined(HELTEC_WIFI_LORA_32_V3)
    #include "bsp/boards/HELTEC_WiFi_LoRa_32_V3/hal_HELTEC_WiFi_LoRa_32_V3.h"
#elif defined(LILYGO_T_LORA_PAGER)
    #include "bsp/boards/LILYGO_T_LoraPager/hal_LILYGO_T_LoraPager.h"
#elif defined(SEEED_SENSECAP_INDICATOR)
    #include "bsp/boards/SEEED_SenseCAP_Indicator/hal_SEEED_SenseCAP_Indicator.h"
#elif defined(ESP32_WITHOUT_LORA)
    #include "bsp/boards/ESP32_Without_LoRa/hal_ESP32_Without_LoRa.h"
#elif defined(ESP32_E22_V1)
    #include "bsp/boards/ESP32_E22_V1/hal_ESP32_E22_V1.h"
#elif defined(SEEED_XIAO_ESP32S3_WIO_SX1262)
    #include "bsp/boards/SEEED_XIAO_ESP32S3_Wio_SX1262/hal_SEEED_XIAO_ESP32S3_Wio_SX1262.h"
#elif defined(LILYGO_T_ETH_ELITE_SX1262)
    #include "bsp/boards/LILYGO_T_ETH_Elite_SX1262/hal_LILYGO_T_ETH_Elite_SX1262.h"
#elif defined(LILYGO_T_ECHO)
    #include "bsp/boards/LILYGO_T_Echo/hal_LILYGO_T_Echo.h"
#else
    #error "No HAL defined for this board!"
#endif


