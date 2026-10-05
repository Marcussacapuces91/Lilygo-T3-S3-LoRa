/**
 * @file application.h
 * @brief Application class for LoRaWAN device management on Lilygo T3-S3
 * 
 * This file defines the Application class which manages the main application logic for a
 * Lilygo T3-S3 LoRa device. It handles:
 * - Radio initialization and LoRaWAN communication
 * - Temperature sensor monitoring
 * - Battery voltage measurement
 * - Real-time clock synchronization
 * - Power management and sleep modes
 * - Device reset reason logging
 * 
 * @author Marcussacapuces91
 * @date 2026
 */

#pragma once

// #include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include "esp_rom_sys.h"
#include "soc/reset_reasons.h"
#include "driver/temperature_sensor.h"
#include "esp_sleep.h"
// #include "esp_pm.h"
// #include "driver/usb_serial_jtag.h"

#include "radio.h"
#include "led.h"
#include "adc.h"

#define TAG_APP "APP"
#define TZ_EUROPE_PARIS "CET-1CEST,M3.5.0,M10.5.0/3"



class Application {
public:
    Application(RadioLoRaWAN& aRadio) :
        radio(aRadio)
        {}

    ~Application() {
        radio.sleep();
    }

    void setup() {
        Serial.begin(115200);
        while (!Serial || millis() < 3000);

        printResetReason();
        initTimezone(TZ_EUROPE_PARIS);
        
        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 40);
        ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_handle));

// Radio 
        if (!radio.begin()) {
            ESP_LOGE(TAG_APP, "Échec de l'initialisation. Reboot du système dans 5 sec.");
            delay(5000);
            esp_restart();
        }

        char payload[50];
        const auto len = snprintf(payload, sizeof(payload), "Start - RR=%d", esp_rom_get_reset_reason(0));
        ESP_LOGE(TAG_APP, "Send device starting status (Reset reason): %s", payload);
        if (!radio.send(10, payload, len, true))   // fPort = 10.
            ESP_LOGE(TAG_APP, "Can't send payload (FPort:10) : %s !", payload);
        for (int i = 1; i < 5; ++i) {
            const time_t now = time(NULL);
            if (now > 1000000000ULL) break; //  9 septembre 2001

            char timeStr[64];
            getLocalTimeFormatted(timeStr, sizeof(timeStr));
            ESP_LOGI(TAG_APP, "Heure locale : %s", timeStr);

            delay(30000);   // 30 sec.
            if (!radio.send(0, NULL, 0, true))
                ESP_LOGE(TAG_APP, "Can't send empty payload (FPort:0)!");

            ESP_LOGI(TAG_APP, "Real time read and processed.");
            getLocalTimeFormatted(timeStr, sizeof(timeStr));
            ESP_LOGI(TAG_APP, "Heure locale : %s", timeStr);
        }

        if (!radio.sleep())
            ESP_LOGE(TAG_APP, "Can't sleep radio!");

        ESP_LOGI(TAG_APP, "Initialisation terminée.");
    }

    void loop() {
        greenLed.off();
        waitForNext(60);
        greenLed.on();

        char timeStr[64];
        getLocalTimeFormatted(timeStr, sizeof(timeStr));
        ESP_LOGI(TAG_APP, "Heure locale : %s", timeStr);

        uint8_t payload[100];
        size_t offset = 0;

        const uint32_t ts = time(NULL);
        ESP_LOGI(TAG_APP, "Timestamp: %d", ts);
        payload[offset++] = 0x00;      // channel 0
        payload[offset++] = 0x85;      // unix Timestamp + 4 bytes
        payload[offset++] = (uint8_t)(ts >> 24);
        payload[offset++] = (uint8_t)(ts >> 16);
        payload[offset++] = (uint8_t)(ts >> 8);
        payload[offset++] = (uint8_t)(ts & 0xFF);

        const float v = vbat.read();
        ESP_LOGI(TAG_APP, "Voltage batterie %f V", v);
        const uint16_t batVal = (int16_t)round(v * 100.0f);

        payload[offset++] = 0x00;     // Channel
        payload[offset++] = 0x02;     // Analog Input
        payload[offset++] = (uint8_t)(batVal >> 8);
        payload[offset++] = (uint8_t)(batVal & 0xFF);

        if (!radio.standby())
            ESP_LOGE(TAG_APP, "Echec de radio.standby() !");
        else {
            if (!radio.send(1, payload, offset))
                ESP_LOGE(TAG_APP, "Echec de radio.send de la payload!");
            else
                ESP_LOGI(TAG_APP, "payload envoyée.");

            if (!radio.sleep())
                ESP_LOGE(TAG_APP, "Echec de radio.sleep()!");
        }



/*
        constexpr auto BUFFER_SIZE = 15;

        float temp[BUFFER_SIZE];
        for (int t = 0; t < BUFFER_SIZE ; ++t) {
            delay(60000);

            char timeStr[64];
            getLocalTimeFormatted(timeStr, sizeof(timeStr));
            ESP_LOGI(TAG_APP, "Heure courante : %s", timeStr);

            ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));
            
            ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, temp + t));
            ESP_LOGI(TAG_APP, "Temperature in %f °C", temp[t]);
            ESP_ERROR_CHECK(temperature_sensor_disable(temp_handle));
        }

        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        uint8_t payload[6 + BUFFER_SIZE * 4 + 3 + 4];
        unsigned offset = 0;

        uint32_t ts = (uint32_t)now - BUFFER_SIZE * 60;
        payload[offset++] = 0x00;      // channel 0
        payload[offset++] = 0x85;      // unix Timestamp + 4 bytes
        payload[offset++] = (uint8_t)(ts >> 24);
        payload[offset++] = (uint8_t)(ts >> 16);
        payload[offset++] = (uint8_t)(ts >> 8);
        payload[offset++] = (uint8_t)(ts & 0xFF);

        for (int t = 0 ; t < BUFFER_SIZE; ++t) {
            payload[offset++] = t;     // Channel
            payload[offset++] = 0x67;  // Data type temp. sensor : 2 bytes 0.1 signed MSB
            const int16_t tempVal = (int16_t)round(temp[t] * 10.0f);
            payload[offset++] = (uint8_t)(tempVal >> 8);
            payload[offset++] = (uint8_t)(tempVal & 0xFF);
        }

        payload[offset++] = 0x00;     // Channel
        payload[offset++] = 0x00;     // Digital Input
        payload[offset++] = uint8_t(now > 1700000000);    // Time set

        const int raw = adc1_get_raw(ADC1_CHANNEL_0);
        // Conversion en tension (approximation)
        const float voltage = raw * (3.6 / 4095.0);   // tension mesurée = VBAT/2
        const float vbat = voltage * 2.0;             // tension batterie réelle
        ESP_LOGI(TAG_APP, "Voltage batterie %f V", vbat);
        const uint16_t batVal = (int16_t)round(vbat * 100.0f);

        payload[offset++] = 0x00;     // Channel
        payload[offset++] = 0x02;     // Analog Input
        payload[offset++] = (uint8_t)(batVal >> 8);
        payload[offset++] = (uint8_t)(batVal & 0xFF);


        // Demande de synchro temporelle au serveur réseau
        if ((now <= 1700000000) || (timeinfo.tm_min == 0)) {
            ESP_LOGI(TAG_APP, "Demande RADIOLIB_LORAWAN_MAC_DEVICE_TIME (Year: %d)", timeinfo.tm_year);
            // radio.ReqMACDeviceTime();
        }

        int state = loraNode.sendReceive(payload, offset, 1, false);
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

        radio.sleep(true);
*/
    }

protected:
    void waitForNext(const int& interval) const {
        ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON));
        // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);

        ESP_LOGI(TAG_APP, "Go to sleep %d sec...", interval);
        const int64_t t_before_us = esp_timer_get_time();
        const int64_t next_minute_us = (t_before_us / (interval * 1000000ULL) + 1) * (interval * 1000000ULL);
        const uint64_t delta_us = next_minute_us - t_before_us;

#if 1
        delay(delta_us/1000);
#else
        ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(delta_us));
        ESP_ERROR_CHECK(esp_light_sleep_start());
#endif

        // usb_serial_jtag_driver_config_t usb_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        // usb_serial_jtag_driver_install(&usb_config);
        // // esp_vfs_dev_usb_serial_jtag_use_driver();
        // esp_log_set_vprintf(&vprintf);
        // delay(2000);

        ESP_LOGI(TAG_APP, "Woked up!");

    }

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

    void getLocalTimeFormatted(char buffer[], const size_t bufferSize) {
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", &timeinfo);
    }

private:
    RadioLoRaWAN& radio;
    temperature_sensor_handle_t temp_handle;
    Led<GPIO_NUM_37> greenLed;
    VBat vbat;
};
