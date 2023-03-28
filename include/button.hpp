#pragma once

#include "common.h"
#include <driver/gpio.h>
#include <freertos/task.h>

class Button: public Base{
public:
    Button(gpio_num_t pin): Base("Button"), pin(pin) {
        ESP_LOGD(TAG, "Init button %d", pin);
        const gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK){
            ESP_LOGE(TAG, "Button %d not configured. Error: %d", pin, err);
        }
        ESP_LOGI(TAG, "Button inited %d", pin);
    }

    int level(){
        return gpio_get_level(pin);
    }

    bool is_long_pressed(){
        for (int i=0; i<4; i++){
            if (level()) return false;
            delay(1000);
        }
        return true;
    }

private:
    gpio_num_t pin;
};