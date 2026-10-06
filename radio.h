
#pragma once

#include <RadioLib.h>
// #define RADIO_BOARD_AUTO
// #include <RadioBoards.h>

#include <SPI.h>
#include <stdio.h>
#include <esp_mac.h>

#define TAG_RADIO "RADIO"

/*
#define I2C_SDA                     21
#define I2C_SCL                     22
#define OLED_RST                    UNUSED_PIN

#define RADIO_SCLK_PIN              5
#define RADIO_MISO_PIN              19
#define RADIO_MOSI_PIN              27
#define RADIO_CS_PIN                18
#define RADIO_DIO0_PIN               26
#define RADIO_RST_PIN               14
#define RADIO_DIO1_PIN              33

// SX1276/78
#define RADIO_DIO2_PIN              32
// SX1262
#define RADIO_BUSY_PIN              32
*/

// // --- Configuration Pins LilyGO T3-S3 ---
// namespace Pins {
//     constexpr uint8_t SPI_SCK  = 5;
//     constexpr uint8_t SPI_MISO = 3;
//     constexpr uint8_t SPI_MOSI = 6;
//     constexpr uint8_t SPI_CS   = 7;

//     // constexpr uint8_t LORA_DIO1 = 33;
//     // constexpr uint8_t LORA_RST  = 8;
//     // constexpr uint8_t LORA_BUSY = 34;
// }



// --- Configuration LoRaWAN ABP (TTN) ---
namespace LoRaConfig {
#include "keys.h"
}

class RadioLoRaWAN {
public:
/**
 * Constructeur de radio.
 * @param addr Adresse déclarée du device (#id).
 */
    RadioLoRaWAN(const uint32_t addr = 0) :
        dev_addr(addr),
        spiBus(FSPI),
        radioModule(new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, spiBus)),
//        radioModule(new RadioModule()),
        loraNode(&radioModule, &EU868)
    {
        ESP_LOGD(TAG_RADIO, "Instanciate LoRaWAN end-device 0x%08X", addr);
    }

    ~RadioLoRaWAN() {
        const auto state = radioModule.sleep();
        if (state != RADIOLIB_ERR_NONE)
            ESP_LOGW(TAG_RADIO, "Échec radioModule.sleep() : %s", errorMessage(state));
    }

    bool begin() {
        return initRadio(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS, 868.1) && initLoRaWAN(dev_addr);
    }

    bool send(const uint8_t& fPort, const void* payload, const size_t& size, const bool& ack = false) {
        const time_t now = time(NULL);
        if (now < 1000000000ULL) {  // 9 septembre 2001
            ESP_LOGI(TAG_RADIO, "Real time unset (%d): reqMACDeviceTime.", now);
            if (!reqMACDeviceTime())
                ESP_LOGI(TAG_RADIO, "Echec de reqMACDeviceTime ");
        }
        const auto state = loraNode.sendReceive((const uint8_t*)payload, size, fPort, ack);   // false = no ack
        if (state >= RADIOLIB_ERR_NONE) {
            processDeviceTimeResponse();
            if (state > 0)
                ESP_LOGI(TAG_RADIO, "Downlink reçu (%d octets) mais non-lu.", state);
            return true;
        }
        ESP_LOGE(TAG_RADIO, "Echec de loraNode.sendReceive : %s", errorMessage(state));
        return false;
    }

    bool reqMACDeviceTime() {
        const auto state = loraNode.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
        if (state != RADIOLIB_ERR_NONE)
            ESP_LOGE(TAG_RADIO, "Échec RADIOLIB_LORAWAN_MAC_DEVICE_TIME : %s", errorMessage(state));
        return state == RADIOLIB_ERR_NONE;
    }

    bool processDeviceTimeResponse() const {
        uint32_t uTimestamp = 0;
        uint16_t milliseconds = 0;

        const auto state = const_cast<LoRaWANNode&>(loraNode).getMacDeviceTimeAns(&uTimestamp, &milliseconds, true);
        if ((state == RADIOLIB_ERR_NONE) && (uTimestamp > 0)) {
            const struct timeval tv = {
                .tv_sec = (time_t)uTimestamp,
                .tv_usec = milliseconds * 1000
            };
            settimeofday(&tv, NULL);
            // struct tm tm_utc;
            // gmtime_r((time_t*)(&uTimestamp), &tm_utc);
            // char uTime[64];
            // strftime(uTime, sizeof(uTime), "%Y-%m-%d %H:%M:%S", &tm_utc);
            // ESP_LOGI(TAG_RADIO, "Horloge système mise à jour depuis TTN : %s.%03u", uTime, milliseconds);
            return true;
        } else {
            ESP_LOGD(TAG_RADIO, "Pas de réponse DeviceTimeAns sur cette fenêtre.");
            return false;
        }
    }

/**
 * Switch radio module to sleep state. Context is saved.
 * Use standby to wakeup. @see `RadioLoRaWAN::standby()`.
 * @return `true` if success or else `false`.
 */
    bool sleep() {
        const auto state = radioModule.sleep(true);
        if (state != RADIOLIB_ERR_NONE)
            ESP_LOGD(TAG_RADIO, "Échec radioModule.sleep() : %s", errorMessage(state));
        return (state == RADIOLIB_ERR_NONE);
    }

/**
 * Wake the rabio module up. Don't need to run `RadioLoRaWAN.begin()` again.
 * @return `true` if success or else `false`.
 */
    bool standby() {
        const auto state = radioModule.standby();
        if (state != RADIOLIB_ERR_NONE)
            ESP_LOGD(TAG_RADIO, "Échec radioModule.standby() : %s", errorMessage(state));
        return (state == RADIOLIB_ERR_NONE);
    }

protected:
/**
 * @see RadioLib/src/TypeDef.h
 * @see https://github.com/jgromes/RadioLib/blob/02f0afaaa32de07874e2954dd929e8c08356e24f/src/TypeDef.h
 * @see https://jgromes.github.io/RadioLib/group__status__codes.html
 * @param error: error number (< 0)
 * @return pointer on error message string.
 */
    const char* errorMessage(const int& error) const {
        static char message[25];
        switch (error) {
            case RADIOLIB_ERR_NONE:                        // 0 = Ok
                return "No error.";
            case RADIOLIB_ERR_UNKNOWN:                     // -1
 	            return "There was an unexpected, unknown error. If you see this, something went incredibly wrong. Your Arduino may be possessed, contact your local exorcist to resolve this error.";
            case RADIOLIB_ERR_CHIP_NOT_FOUND:              // -2
                return "Radio chip was not found during initialization. This can be caused by specifying wrong chip type in the constructor (i.e. calling SX1272 constructor for SX1278 chip) or by a fault in your wiring (incorrect slave select pin).";
            case RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED:    // -3
 	            return "Failed to allocate memory for temporary buffer. This can be cause by not enough RAM or by passing invalid pointer.";
            case RADIOLIB_ERR_PACKET_TOO_LONG:             // -4
                return "Packet supplied to transmission method was longer than limit.";
            case RADIOLIB_ERR_TX_TIMEOUT:                  // -5
 	            return "Timed out waiting for transmission finish.";
            case RADIOLIB_ERR_RX_TIMEOUT:                  // -6
                return "Timed out waiting for incoming transmission.";
            case RADIOLIB_ERR_CRC_MISMATCH:                // -7
                return "The calculated and expected CRCs of received packet do not match. This means that the packet was damaged during transmission and should be sent again.";
            case RADIOLIB_ERR_INVALID_BANDWIDTH:           // -8
                return "The supplied bandwidth value is invalid for this module.";
            case RADIOLIB_ERR_INVALID_SPREADING_FACTOR:    // -9
                return "The supplied spreading factor value is invalid for this module.";
            case RADIOLIB_ERR_INVALID_CODING_RATE:         // -10
                return "The supplied coding rate value is invalid for this module.";
            // -11 RADIOLIB_ERR_INVALID_BIT_RANGE (-11) Internal only.
            case RADIOLIB_ERR_INVALID_FREQUENCY:           // -12
                return "The supplied frequency value is invalid for this module.";
            case RADIOLIB_ERR_INVALID_OUTPUT_POWER:        // -13
                return "The supplied frequency value is invalid for this module.";
            case RADIOLIB_ERR_NETWORK_NOT_JOINED:
                return "RADIOLIB_ERR_NETWORK_NOT_JOINED";
            case RADIOLIB_ERR_DOWNLINK_MALFORMED:
                return "RADIOLIB_ERR_DOWNLINK_MALFORMED";
            case RADIOLIB_ERR_INVALID_REVISION:
                return "RADIOLIB_ERR_INVALID_REVISION";
            case RADIOLIB_ERR_INVALID_PORT:             // -1104
                return "Invalid LoRaWAN uplink port requested by user, or downlink received at invalid port.";
            case RADIOLIB_ERR_NO_RX_WINDOW:
                return "RADIOLIB_ERR_NO_RX_WINDOW";
            case RADIOLIB_ERR_INVALID_CID:
                return "RADIOLIB_ERR_INVALID_CID";
            case RADIOLIB_ERR_UPLINK_UNAVAILABLE:
                return "RADIOLIB_ERR_UPLINK_UNAVAILABLE";
            case RADIOLIB_ERR_COMMAND_QUEUE_FULL:       // -1109
                return "Unable to push new MAC command because the queue is full.";
            case RADIOLIB_ERR_COMMAND_QUEUE_ITEM_NOT_FOUND:
                return "RADIOLIB_ERR_COMMAND_QUEUE_ITEM_NOT_FOUND";
            case RADIOLIB_ERR_JOIN_NONCE_INVALID:
                return "RADIOLIB_ERR_JOIN_NONCE_INVALID";
            case RADIOLIB_ERR_DWELL_TIME_EXCEEDED:
                return "RADIOLIB_ERR_DWELL_TIME_EXCEEDED";
            case RADIOLIB_ERR_CHECKSUM_MISMATCH:
                return "RADIOLIB_ERR_CHECKSUM_MISMATCH";
            case RADIOLIB_ERR_NO_JOIN_ACCEPT:
                return "RADIOLIB_ERR_NO_JOIN_ACCEPT";
            case RADIOLIB_LORAWAN_SESSION_RESTORED:
                return "RADIOLIB_LORAWAN_SESSION_RESTORED";
            case RADIOLIB_LORAWAN_NEW_SESSION:          // -1118
                return "New session (not an error).";
            case RADIOLIB_ERR_NONCES_DISCARDED:
                return "RADIOLIB_ERR_NONCES_DISCARDED";
            case RADIOLIB_ERR_SESSION_DISCARDED:
                return "RADIOLIB_ERR_SESSION_DISCARDED";
            default:
                snprintf(message, sizeof(message), "Unknown error: %d", error);
                return message;
        }
    }

/**
*  Initialise Radio Lora.
*  @return True si la radio est correctement initialisée.
*/
bool initRadio(const int& spi_sck, const int& spi_miso, const int& spi_mosi, const int& spi_cs, const float& freq) {
    ESP_LOGI(TAG_RADIO, "Initialisation bus SPI matériel...");
    if (!spiBus.begin(spi_sck, spi_miso, spi_mosi, spi_cs)) {
        ESP_LOGE(TAG_RADIO, "Échec de l'init du bus SPI");
        return false;
    }

    ESP_LOGI(TAG_RADIO, "Initialisation RF SX1262...");
    ConfigLoRa_t config = {
        .frequency = freq
    };
    
    auto state = radioModule.begin(config);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG_RADIO, "Échec radio.begin() : %s", errorMessage(state));
        return false;
    }

    // state = radioModule.setOutputPower(14);   // 14 ?
    // if (state != RADIOLIB_ERR_NONE) {
    //     ESP_LOGE(TAG_RADIO, "Échec radio.setOutputPower() : %d", state);
    //     return false;
    // }

    return true;
}

/**
 * Initialise la confoguration LoRaWAN.
 * @return true si l'initialisation s'est bien passée, false sinon.
 */
    bool initLoRaWAN(const uint32_t dev_addr = 0) {
        ESP_LOGI(TAG_RADIO, "Configuration LoRaWAN ABP...");

        auto addr = dev_addr;

        if (!addr) {
            uint8_t mac[8];
            ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
            addr = ((mac[0] * 256 + mac[1]) * 256 + mac[2]) * 256 + mac[3];
        }
        ESP_LOGI(TAG_RADIO, "device address: 0x%08x", addr);

        auto state = loraNode.beginABP(
            addr,
            const_cast<uint8_t*>(LoRaConfig::FN_NWK_S_INT_KEY),
            const_cast<uint8_t*>(LoRaConfig::SN_NWK_S_INT_KEY),
            const_cast<uint8_t*>(LoRaConfig::NWK_S_ENC_KEY),
            const_cast<uint8_t*>(LoRaConfig::APP_S_KEY)
        );

        if (state != RADIOLIB_ERR_NONE) {
            ESP_LOGE(TAG_RADIO, "Échec loraNode.beginABP() : %s", errorMessage(state));
            return false;
        }

        state = loraNode.activateABP();
        if (state != RADIOLIB_ERR_NONE && state != RADIOLIB_LORAWAN_NEW_SESSION) {
            ESP_LOGE(TAG_RADIO, "Échec loraNode.activateABP() : %s", errorMessage(state));
            return false;
        }

        loraNode.setADR(true);

        // loraNode.setRx2Dr(0); // SF12 sur RX2 en EU868 par défaut

        return true;
    }

private:
    uint32_t dev_addr;
    SPIClass spiBus;
    SX1262 radioModule;
    // Radio radioModule;
    LoRaWANNode loraNode;
};
