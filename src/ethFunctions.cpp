#ifdef HAS_ETHERNET

#include <SPI.h>
#include <ETH.h>
#include <WiFi.h>

#include "ethFunctions.h"
#include "hal.h"
#include "settings.h"
#include "udp.h"
#include "logging.h"

static SPIClass ethSPI(FSPI);
bool ethConnected = false;

void ethInit() {
    if (!ethEnabled) {
        logPrintf(LOG_INFO, "ETH", "Ethernet disabled in settings.");
        return;
    }

    // Register ETH-specific network events
    WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
        switch (event) {
            case ARDUINO_EVENT_ETH_START:
                logPrintf(LOG_INFO, "ETH", "Started");
                {
                    String host = String(settings.mycall) + "-rmesh";
                    host.toLowerCase();
                    ETH.setHostname(host.c_str());
                }
                break;
            case ARDUINO_EVENT_ETH_CONNECTED:
                logPrintf(LOG_INFO, "ETH", "Link up (%u Mbps, %s)",
                          ETH.linkSpeed(),
                          ETH.fullDuplex() ? "full-duplex" : "half-duplex");
                break;
            case ARDUINO_EVENT_ETH_GOT_IP:
                logPrintf(LOG_INFO, "ETH", "IP: %s  GW: %s  Mask: %s",
                          ETH.localIP().toString().c_str(),
                          ETH.gatewayIP().toString().c_str(),
                          ETH.subnetMask().toString().c_str());
                ethConnected = true;
                initUDP();
                // Set ETH as default outbound route if configured as primary
                if (primaryInterface == 2) {
                    ETH.setDefault();
                    logPrintf(LOG_INFO, "ETH", "Set as primary interface (outbound route)");
                }
                break;
            case ARDUINO_EVENT_ETH_DISCONNECTED:
                logPrintf(LOG_WARN, "ETH", "Link down");
                ethConnected = false;
                break;
            default:
                break;
        }
    });

    // Configure static IP before begin() if DHCP is disabled
    if (!ethDhcp) {
        ETH.config(ethIP, ethGateway, ethNetMask, ethDNS);
    }

    ethSPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI, -1);

    if (!ETH.begin(ETH_PHY_W5500, ETH_PHY_ADDR, ETH_SPI_CS, ETH_INT_PIN, ETH_RST_PIN, ethSPI)) {
        logPrintf(LOG_ERROR, "ETH", "W5500 init failed – check wiring!");
    }
}

#endif // HAS_ETHERNET
