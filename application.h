#include <SPI.h>
#include <time.h>
#include <sys/time.h>
#include "esp_rom_sys.h"
#include "soc/reset_reasons.h"
#include <RadioLib.h>
#include "driver/temperature_sensor.h"

#define TAG_APP "APP"
#define TZ_EUROPE_PARIS "CET-1CEST,M3.5.0,M10.5.0/3"

// --- Configuration Pins LilyGO T3-S3 ---
namespace Pins {
    constexpr uint8_t SPI_SCK  = 5;
    constexpr uint8_t SPI_MISO = 3;
    constexpr uint8_t SPI_MOSI = 6;
    constexpr uint8_t SPI_CS   = 7;

    // constexpr uint8_t LORA_DIO1 = 33;
    // constexpr uint8_t LORA_RST  = 8;
    // constexpr uint8_t LORA_BUSY = 34;
}

// --- Configuration LoRaWAN ABP (TTN) ---
namespace LoRaConfig {
    constexpr uint32_t DEV_ADDR = 0x260B5C0E;

    // provide your own keys (see on TTN end-device)
    const uint8_t FN_NWK_S_INT_KEY[] = {  };
    const uint8_t SN_NWK_S_INT_KEY[] = {  };
    const uint8_t NWK_S_ENC_KEY[] = {  };
    const uint8_t APP_S_KEY[] = {  };
}

class Application {
public:
    Application() 
        : spiBus(FSPI),
          radio(new Module(Pins::SPI_CS, LORA_DIO1, LORA_RST, LORA_BUSY, spiBus)),
          loraNode(&radio, &EU868) {}

    ~Application() {
        // delete radio.getMod();
    }

    void setup() {
        Serial.begin(115200);
        while (!Serial || millis() < 3000);

        initTimezone(TZ_EUROPE_PARIS);
        printResetReason();

        if (!initRadio() || !initLoRaWAN()) {   // attention à l'ordre !
            ESP_LOGE(TAG_APP, "Échec de l'initialisation. Reboot du système dans 5 sec.");
            delay(5000);
            esp_restart();
        }

        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 40);
        ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_handle));

        ESP_LOGI(TAG_APP, "Initialisation réussie !");
        delay(2000);
    }

    void loop() {
        char timeStr[64];
        getLocalTimeFormatted(timeStr, sizeof(timeStr));
        ESP_LOGI(TAG_APP, "Heure courante : %s", timeStr);

        ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));
        float tsens_out;
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, &tsens_out));
        ESP_LOGI(TAG_APP, "Temperature in %f °C", tsens_out);
        ESP_ERROR_CHECK(temperature_sensor_disable(temp_handle));

        // Demande de synchro temporelle au serveur réseau
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        if ((timeinfo.tm_year <= 70) || (timeinfo.tm_min == 0)) {
            ESP_LOGI(TAG_APP, "Demande RADIOLIB_LORAWAN_MAC_DEVICE_TIME (Year: %d)", timeinfo.tm_year);
            loraNode.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
        }

        uint8_t payload[] = {
            0x01,   // channel 1
            0x67,   // Data type temp. sensor : 2 bytes 0.1 signed MSB
            uint8_t(unsigned(tsens_out * 10) >> 8) & 0x7F,  // TODO traiter < 0
            uint8_t(unsigned(tsens_out * 10)),

            0x01,
            0x00,
            uint8_t(timeinfo.tm_year > 70)
        };

        int state = loraNode.sendReceive(payload, sizeof(payload), 1, false);

        if (state >= RADIOLIB_ERR_NONE) {
            ESP_LOGI(TAG_APP, "Trame transmise avec succès.");
            if (state > 0) {
                ESP_LOGI(TAG_APP, "Downlink reçu (%d octets) mais non-lu.", state);
            }
        } else {
            ESP_LOGE(TAG_APP, "Erreur d'émission sendReceive : %d", state);
        }

        // Tente de récupérer la réponse DeviceTimeAns
        processDeviceTimeResponse();

        delay(60000);
    }

protected:
/**
 * Définit la time Zone applicable aux affichage de daet et heure
 * @param tz Une chaine définissant la Time Zone, UTC par défaut.
 */
    void initTimezone(const char tz[] = "") {
        setenv("TZ", tz, 1);
        tzset();
    }

/**
 * Affiche le log (INFO) de la Reset Reason du dernier reboot.
 */
    void printResetReason() {
    // RESET_REASON_CHIP_POWER_ON   = 0x01, // Power on reset
    // RESET_REASON_CHIP_BROWN_OUT  = 0x01, // VDD voltage is not stable and resets the chip
    // RESET_REASON_CHIP_SUPER_WDT  = 0x01, // Super watch dog resets the chip
    // RESET_REASON_CORE_SW         = 0x03, // Software resets the digital core by RTC_CNTL_SW_SYS_RST
    // RESET_REASON_CORE_DEEP_SLEEP = 0x05, // Deep sleep reset the digital core
    // RESET_REASON_CORE_MWDT0      = 0x07, // Main watch dog 0 resets digital core
    // RESET_REASON_CORE_MWDT1      = 0x08, // Main watch dog 1 resets digital core
    // RESET_REASON_CORE_RTC_WDT    = 0x09, // RTC watch dog resets digital core
    // RESET_REASON_CPU0_MWDT0      = 0x0B, // Main watch dog 0 resets CPU 0
    // RESET_REASON_CPU1_MWDT0      = 0x0B, // Main watch dog 0 resets CPU 1
    // RESET_REASON_CPU0_SW         = 0x0C, // Software resets CPU 0 by RTC_CNTL_SW_PROCPU_RST
    // RESET_REASON_CPU1_SW         = 0x0C, // Software resets CPU 1 by RTC_CNTL_SW_APPCPU_RST
    // RESET_REASON_CPU0_RTC_WDT    = 0x0D, // RTC watch dog resets CPU 0
    // RESET_REASON_CPU1_RTC_WDT    = 0x0D, // RTC watch dog resets CPU 1
    // RESET_REASON_SYS_BROWN_OUT   = 0x0F, // VDD voltage is not stable and resets the digital core
    // RESET_REASON_SYS_RTC_WDT     = 0x10, // RTC watch dog resets digital core and rtc module
    // RESET_REASON_CPU0_MWDT1      = 0x11, // Main watch dog 1 resets CPU 0
    // RESET_REASON_CPU1_MWDT1      = 0x11, // Main watch dog 1 resets CPU 1
    // RESET_REASON_SYS_SUPER_WDT   = 0x12, // Super watch dog resets the digital core and rtc module
    // RESET_REASON_SYS_CLK_GLITCH  = 0x13, // Glitch on clock resets the digital core and rtc module
    // RESET_REASON_CORE_EFUSE_CRC  = 0x14, // eFuse CRC error resets the digital core
    // RESET_REASON_CORE_USB_UART   = 0x15, // USB UART resets the digital core
    // RESET_REASON_CORE_USB_JTAG   = 0x16, // USB JTAG resets the digital core
    // RESET_REASON_CORE_PWR_GLITCH = 0x17, // Glitch on power resets the digital core

        const auto reason = esp_rom_get_reset_reason(0);
        switch (reason) {
            case RESET_REASON_CHIP_POWER_ON:
                ESP_LOGI(TAG_APP, "Power on reset"); break;
            case RESET_REASON_CORE_SW:
                ESP_LOGI(TAG_APP, "Software reset digital core"); break;
            case RESET_REASON_CORE_DEEP_SLEEP:  // 0x05
                ESP_LOGI(TAG_APP, "Deep Sleep reset"); break;

            case RESET_REASON_SYS_CLK_GLITCH:   // 0x13
                ESP_LOGI(TAG_APP, "Glitch on clock resets the digital core and rtc module"); break;
            case RESET_REASON_CORE_EFUSE_CRC:   // 0x14
                ESP_LOGI(TAG_APP, "eFuse CRC error resets the digital core"); break;
            case RESET_REASON_CORE_USB_UART:    // 0x15
                ESP_LOGI(TAG_APP, "USB UART resets the digital core"); break;
            case RESET_REASON_CORE_USB_JTAG:    // 0x16
                ESP_LOGI(TAG_APP, "USB JTAG resets the digital core"); break;
            case RESET_REASON_CORE_PWR_GLITCH:  // 0x17
                ESP_LOGI(TAG_APP, "Glitch on power resets the digital core"); break;
            default: 
                ESP_LOGE(TAG_APP, "Reset reason ESP32 (code %d)", reason);
        }
    }

/**
 *  Initialise Radio LoraWan.
 *  @return True si la radio est correctement initialisée.
 */
    bool initRadio() {
        ESP_LOGI(TAG_APP, "Initialisation bus SPI matériel...");
        if (!spiBus.begin(Pins::SPI_SCK, Pins::SPI_MISO, Pins::SPI_MOSI, Pins::SPI_CS)) {
            ESP_LOGE(TAG_APP, "Échec de l'init du bus SPI");
            return false;
        }

        ESP_LOGI(TAG_APP, "Initialisation RF SX1262...");
        ConfigLoRa_t config = {
            .frequency = 868.1
        };
        
        int state = radio.begin(config);
        if (state != RADIOLIB_ERR_NONE) {
            ESP_LOGE(TAG_APP, "Échec radio.begin() : %d", state);
            return false;
        }

        state = radio.setOutputPower(10);   // 14 ?
        if (state != RADIOLIB_ERR_NONE) {
            ESP_LOGE(TAG_APP, "Échec radio.setOutputPower() : %d", state);
            return false;
        }

        return true;
    }

/**
 * Initialise la confoguration LoRaWAN.
 * @return true si l'initialisation s'est bien passée, false sinon.
 */
    bool initLoRaWAN() {
        ESP_LOGI(TAG_APP, "Configuration LoRaWAN ABP...");

        int state = loraNode.beginABP(
            LoRaConfig::DEV_ADDR,
            const_cast<uint8_t*>(LoRaConfig::FN_NWK_S_INT_KEY),
            const_cast<uint8_t*>(LoRaConfig::SN_NWK_S_INT_KEY),
            const_cast<uint8_t*>(LoRaConfig::NWK_S_ENC_KEY),
            const_cast<uint8_t*>(LoRaConfig::APP_S_KEY)
        );

        if (state != RADIOLIB_ERR_NONE) {
            ESP_LOGE(TAG_APP, "Échec loraNode.beginABP() : %d", state);
            return false;
        }

        state = loraNode.activateABP();
        if (state != RADIOLIB_ERR_NONE && state != RADIOLIB_LORAWAN_NEW_SESSION) {
            ESP_LOGE(TAG_APP, "Échec loraNode.activateABP() : %d", state);
            return false;
        }

        loraNode.setADR(true);
        loraNode.setRx2Dr(0); // SF12 sur RX2 en EU868 par défaut

        return true;
    }

    void processDeviceTimeResponse() {
        uint32_t gpsTimestamp = 0;
        uint16_t milliseconds = 0;

        int state = loraNode.getMacDeviceTimeAns(&gpsTimestamp, &milliseconds, true);
        
        if (state == RADIOLIB_ERR_NONE && gpsTimestamp > 0) {
            const struct timeval tv = {
                .tv_sec = (time_t)gpsTimestamp,
                .tv_usec = milliseconds * 1000
            };
            settimeofday(&tv, NULL);

            char updatedTime[64];
            getLocalTimeFormatted(updatedTime, sizeof(updatedTime));
            ESP_LOGI(TAG_APP, "Horloge système mise à jour depuis TTN : %s.%03u", updatedTime, milliseconds);
        } else {
            ESP_LOGD(TAG_APP, "Pas de réponse DeviceTimeAns sur cette fenêtre.");
        }
    }

    void getLocalTimeFormatted(char buffer[], const size_t bufferSize) {
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", &timeinfo);
    }

private:
    SPIClass spiBus;
    SX1262 radio;
    LoRaWANNode loraNode;
    temperature_sensor_handle_t temp_handle;
};
