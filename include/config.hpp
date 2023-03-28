#pragma once

#include "common.h"
#include "led.hpp"
#include <nvs_flash.h>
#include <nvs_handle.hpp>
#include <freertos/task.h>
#include <string>

class Config:public Base{
public:
    Config(Led led): Base("Config"), _led(led) {
        ESP_LOGD(TAG, "Init config");
        esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }
        if (err != ESP_OK){
            halt("init failed", err);
        }
        read_config();
        ESP_LOGI(TAG, "Config inited");
    }

    void halt(const char* msg, esp_err_t err){
        int i=0;
        while(true){
            _led.toggle();
            delay(100);
            if (i++ % 10 == 0){
                ESP_LOGE(TAG, "NVS error (%d): %s", err, msg);
            }
        }
    }

    void check(esp_err_t err, const char* msg){
        if(err != ESP_OK){
            ESP_LOGE(TAG, "Error in %s: %d\n", msg, err);
        }
    }

    void read_config(){
        ESP_LOGI(TAG, "Read config");
        esp_err_t err;
        std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("config", NVS_READWRITE, &err);
        if (err != ESP_OK){
            halt("open failed", err);
        }
        _ssid = read_string(handle.get(), "ssid");
        _pswd = read_string(handle.get(), "pswd");
        check(handle->get_item<uint16_t>("port", _port), "read port");
        check(handle->get_item<uint32_t>("uart", uart), "read uart pins");
        check(handle->get_item<uint16_t>("screen", screen), "read screen pins");
        check(handle->get_item<uint16_t>("ina", ina), "read INA pins");
    }

    void save_config(){
        esp_err_t err;
        std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("config", NVS_READWRITE, &err);
        if (err != ESP_OK){
            halt("open config failed", err);
        }
        check(handle->set_string("ssid", _ssid.c_str()), "write ssid");
        check(handle->set_string("pswd", _pswd.c_str()), "write pswd");
        check(handle->set_item<uint16_t>("port", _port), "write port");
        check(handle->set_item<uint32_t>("uart", uart), "write uart pins");
        check(handle->set_item<uint16_t>("screen", screen), "write screen pins");
        check(handle->set_item<uint16_t>("ina", ina), "write INA pins");
        check(handle->commit(), "write commit");
    }

    std::string read_string(nvs::NVSHandle* handle, const char* name){
        std::string ret;
        size_t len = 0;
        esp_err_t err = handle->get_item_size(nvs::ItemType::SZ, name, len);
        if (err != ESP_OK) return ret;
        ret.resize(len);
        err = handle->get_string(name, &ret[0], len);
        if(err != ESP_OK){
            halt("cant read value ", err);
        }
        if (ret[len-1] == 0){
            ret.resize(len-1);
        }
        return ret;
    }

    bool ready() {
        return !_ssid.empty() && !_pswd.empty() && _port != 0;
    }

    inline Led& led(){return _led;}
    inline std::string& ssid() {return _ssid;}
    inline std::string& pswd() {return _pswd;}
    inline uint16_t port() {return _port;}
    inline uint16_t screen_i2c() {return screen;}
    inline uint16_t ina_i2c() {return ina;}
    inline uint32_t get_uart() {return uart;}

    int get_uart_io(int port){
        return (int)(uart >> (port * 8)) & 0xFF;
    } 

    void set_config_wifi(std::string ssid, std::string pswd, int udp_port){
        _ssid = ssid;
        _pswd = pswd;
        _port = static_cast<uint16_t>(udp_port);
    }
    void set_config_ports(int uartp, int scrp, int inap){
        uart = static_cast<uint32_t>(uartp);
        screen = static_cast<uint16_t>(scrp);
        ina = static_cast<uint16_t>(inap);
    }

private:
    Led _led;
    std::string _ssid;
    std::string _pswd;
    uint16_t _port = 0;
    uint32_t uart = 0;
    uint16_t screen = 0;
    uint16_t ina = 0;
};