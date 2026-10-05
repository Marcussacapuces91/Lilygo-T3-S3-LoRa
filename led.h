#pragma once
#include "driver/gpio.h"
#include "esp_log.h"

template<gpio_num_t GPIO>
class Led {
public:
    Led() {
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&cfg);

        off();
        ESP_LOGI(TAG, "LED on GPIO %d initialized", int(GPIO));
    }

    void on() {
        gpio_set_level(GPIO, 1);
    }

    void off() {
        gpio_set_level(GPIO, 0);
    }

    void toggle() {
        state = !state;
        gpio_set_level(GPIO, state ? 1 : 0);
    }

    bool isOn() const {
        return state;
    }

private:
    static constexpr const char* TAG = "LED";
    bool state = false;
};
