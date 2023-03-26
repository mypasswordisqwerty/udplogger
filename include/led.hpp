#pragma once

#include "common.h"
#include <driver/gpio.h>

class Led: public Base{
public:
    Led(gpio_num_t pin): Base("Led"), pin(pin){
        ESP_LOGD(TAG, "Init led %d", pin);
        const gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK){
            ESP_LOGE(TAG, "Led %d not configured. Error: %d", pin, err);
        }
    }

    void set_level(bool high){
        level = high;
        gpio_set_level(pin, high ? 1 : 0);
    }

    inline void toggle() {
        set_level(!level);
    }
private:
    gpio_num_t pin;
    bool level = false;
};