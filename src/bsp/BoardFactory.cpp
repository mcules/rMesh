#include "BoardFactory.h"

#include "boards/HELTEC_WiFi_LoRa_32_V3/Board_Heltec_V3.h"
#include "boards/HELTEC_WiFi_LoRa_32_V4/Board_Heltec_V4.h"
#include "boards/HELTEC_Wireless_Stick_Lite_V3/Board_Heltec_WirelessStickLite_V3.h"
#include "boards/HELTEC_HT_Tracker_V1_2/Board_Heltec_HT_Tracker_V1_2.h"
#include "boards/LILYGO_T3_LoRa32_V1_6_1/Board_LILYGO_T3_LoRa32_V1_6_1.h"
#include "boards/LILYGO_T_Beam/Board_LILYGO_T_Beam.h"
#include "boards/LILYGO_T_Echo/Board_LILYGO_T_Echo.h"
#include "boards/LILYGO_T_LoraPager/Board_LILYGO_T_LoraPager.h"
#include "boards/LILYGO_T_ETH_Elite/Board_LILYGO_T_ETH_Elite.h"
#include "boards/LILYGO_T_ETH_Elite_SX1262/Board_LILYGO_T_ETH_Elite_SX1262.h"
#include "boards/SEEED_SenseCAP_Indicator/Board_SEEED_SenseCAP_Indicator.h"
#include "boards/SEEED_XIAO_ESP32S3_Wio_SX1262/Board_SEEED_XIAO_ESP32S3_Wio_SX1262.h"
#include "boards/ESP32_E22_V1/Board_ESP32_E22_V1.h"
#include "boards/ESP32_Without_LoRa/Board_ESP32_Without_LoRa.h"

/**
 * @brief Returns the IBoardConfig instance for the compiled target board.
 *
 * THIS IS THE ONLY PLACE IN THE ENTIRE CODEBASE THAT CHECKS BOARD IDENTITY.
 * Board selection is done via build flags in platformio.ini, e.g.:
 *   build_flags = -D HELTEC_WIFI_LORA_32_V3
 */
IBoardConfig* BoardFactory::create() {
#if defined(HELTEC_WIFI_LORA_32_V3)
    return new Board_Heltec_V3();
#elif defined(HELTEC_WIFI_LORA_32_V4)
    return new Board_Heltec_V4();
#elif defined(HELTEC_WIRELESS_STICK_LITE_V3)
    return new Board_Heltec_WirelessStickLite_V3();
#elif defined(HELTEC_HT_TRACKER_V1_2)
    return new Board_Heltec_HT_Tracker_V1_2();
#elif defined(LILYGO_T3_LORA32_V1_6_1)
    return new Board_LILYGO_T3_LoRa32_V1_6_1();
#elif defined(LILYGO_T_BEAM)
    return new Board_LILYGO_T_Beam();
#elif defined(LILYGO_T_ECHO)
    return new Board_LILYGO_T_Echo();
#elif defined(LILYGO_T_LORA_PAGER)
    return new Board_LILYGO_T_LoraPager();
#elif defined(LILYGO_T_ETH_ELITE)
    return new Board_LILYGO_T_ETH_Elite();
#elif defined(LILYGO_T_ETH_ELITE_SX1262)
    return new Board_LILYGO_T_ETH_Elite_SX1262();
#elif defined(SEEED_SENSECAP_INDICATOR)
    return new Board_SEEED_SenseCAP_Indicator();
#elif defined(SEEED_XIAO_ESP32S3_WIO_SX1262)
    return new Board_SEEED_XIAO_ESP32S3_Wio_SX1262();
#elif defined(ESP32_E22_V1)
    return new Board_ESP32_E22_V1();
#elif defined(ESP32_WITHOUT_LORA)
    return new Board_ESP32_Without_LoRa();
#else
    #error "No board defined. Set a -D BOARD_XYZ build flag in platformio.ini"
#endif
}
