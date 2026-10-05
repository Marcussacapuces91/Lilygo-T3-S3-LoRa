#pragma once

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "soc/adc_channel.h"

class adc {

};

class VBat : public adc {
public:
    VBat() {
        const adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

        const adc_oneshot_chan_cfg_t adc_config = {
            .atten = ADC_ATTEN_DB_12,           // Vref * 2 = 2200 mV
            .bitwidth = ADC_BITWIDTH_DEFAULT    // 12bits
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_0, &adc_config));

        const adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle));
    }

    ~VBat() {} // release cali

    float read() {
        int adc_raw;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_0, &adc_raw));

        int voltage;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage));

        return voltage / 500.0f;  // due to resistors divider.
    }

protected:

private:
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t cali_handle;

};
