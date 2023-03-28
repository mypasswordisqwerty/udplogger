#pragma once

#include "common.h"
#include "i2c.hpp"
#include "config.hpp"
#include "udp.hpp"
#include <math.h>
#include <iomanip>
#include <sstream>

class INA: public I2C{
public:
    INA(Config& config, int avg=50): I2C("INA", 1), avg(avg)
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
        uint8_t cfg[3] = {0, 0x75, 0x27};
        write_bytes(cfg, 3);
        ESP_LOGI(TAG, "INA inited");
    }

    void run(){
        while(true){
            update();
            if (++count >= avg){
                send();
                count = 0;
            }
            delay(100);
        }
    }

private:

    inline double conv_val(uint16_t val){
        uint16_t nval = (val & 0xFF) << 8 | (val >> 8);
        int16_t ival = reinterpret_cast<int16_t&>(nval);
        double minus = ival < 0 ? -1.0 : 1.0;
        if (ival < 0) ival = -ival;
        return (ival >> 3) * minus;
    }

    inline double conv_bus(int16_t val){
        return conv_val(val) * 0.008;
    }

    inline double conv_shunt(int16_t val){
        return conv_val(val) * 0.004 * 50;
    }

    inline std::string format(double val){
        std::string s = std::to_string(val);
        if (s.length()>4){
            s = s.substr(0, 4);
        }
        while (s.length()>0 && (s[s.length()-1] == '0' || s[s.length()-1] == '.')) s = s.substr(0, s.length()-1);
        return s;
    }

    void update() {
        for (uint8_t i=1; i<=6; i++){
            write_bytes(&i, 1);
            read_bytes((uint8_t*)&buf[i-1], 2);
        }
        for (int i=0; i<3; i++){
            double shunt = conv_shunt(buf[i * 2]);
            double bus = conv_bus(buf[i * 2 + 1]); 
            shunt_ma[i] += shunt;
            bus_v[i] += bus;
            Screen::update_label(i*2, format(bus) + "V");
            Screen::update_label(i*2+1, format(shunt) + "mA");
        }
    }

    void send() {
        std::stringstream ss;
        ss << "INA:";
        for (int i=0; i<3; i++){
            shunt_ma[i] /= count;
            bus_v[i] /= count;
            ss << " " << std::fixed << std::setprecision(6) << bus_v[i] << " " << shunt_ma[i];
        }
        ESP_LOGI(TAG, "%s", ss.str().c_str());
        UDP::send(ss.str() + "\n");
    }

private:
    int avg;
    uint8_t reg = 0;
    uint16_t buf[6];
    double shunt_ma[3] = {.0};
    double bus_v[3] = {.0};
    int count = 0;
};
