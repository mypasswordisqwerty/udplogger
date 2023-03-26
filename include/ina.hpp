#pragma once

#include "common.h"
#include "i2c.hpp"
#include "config.hpp"
#include <math.h>

class INA: public I2C{
public:
    INA(Config& config): I2C("INA", 1)
    {
        if (!config.ready()){
            return;
        }
        uint16_t pins = config.ina_i2c();
        ESP_LOGI(TAG, "Init INA to pins scl %d sda %d", pins & 0xFF, (pins >> 8));
        if (!init_i2c(pins & 0xFF, (pins >> 8) & 0xFF, 0x40)) {
            ESP_LOGE(TAG, "INA init failed");
            return;
        }
        ESP_LOGI(TAG, "INA inited");
    }

    inline double conv_val(int16_t val){
        double minus = val < 0 ? -1.0 : 1.0;
        if (val < 0) val = -val;
        return (val >> 3) * minus;
    }

    inline double conv_bus(int16_t val){
        return conv_val(val) * 0.008;
    }

    inline double conv_shunt(int16_t val){
        return conv_val(val) * 0.040;
    }

    inline std::string format(double val){
        return std::to_string(round(val*100)/100.f);
    }

    void update(){
        read_bytes((uint8_t*)buf, 12, &reg, 1);
        for (int i=0; i<3; i++){
            shunt_mv[i] = conv_shunt(buf[i * 2]);
            bus_v[i] = conv_bus(buf[i * 2 + 1]);
            Screen::update_label(i*2, format(bus_v[i]) + "V");
            Screen::update_label(i*2+1, format(shunt_mv[i]) + "mV");
        }
        ESP_LOGI(TAG, "channels 1: %.2fV %.2fmV, 2: %.2fV %.2fmV, 3: %.2fV %.2fmV", 
            bus_v[0], shunt_mv[0], bus_v[1], shunt_mv[1], bus_v[2], shunt_mv[2]);
    }

    void run(){
        while(true){
            ESP_LOGI(TAG, "Update ina");
            update();
            vTaskDelay(1000);
        }
    }

private:
    uint8_t reg = 1;
    int16_t buf[6];
    double shunt_mv[3] = {.0};
    double bus_v[3] = {.0};
};
