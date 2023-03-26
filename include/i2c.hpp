#pragma once

#include "common.h"
#include <driver/i2c.h>

class I2C:public Thread{
    using unique_cmd_t = std::unique_ptr<void, decltype(&i2c_cmd_link_delete)>;

public:
    I2C(std::string name, int port): Thread(name), port(port){
    }

    bool init_i2c(int scl, int sda, uint8_t client) {
        this->client = client << 1;
        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = sda,
            .scl_io_num = scl,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master = {
                .clk_speed = 400000,
            },
            .clk_flags = 0,
        };
        esp_err_t err = i2c_param_config(port, &conf);
        if (err){
            ESP_LOGE(TAG, "I2C config error: %d", err);
            return false;
        }
        err = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
        if (err){
            ESP_LOGE(TAG, "I2C config error: %d", err);
            return false;
        }
        return true;
    }

    bool check_err(esp_err_t err, const char* op){
        if (err != ESP_OK){
            ESP_LOGE(TAG, "Error in i2c write %s: %d %4.4X", op, err, err);
        }
        return err == ESP_OK;
    }

    void write_bytes(const uint8_t* bytes, size_t len){
        unique_cmd_t cmd(i2c_cmd_link_create(), i2c_cmd_link_delete);
        esp_err_t err = i2c_master_start(cmd.get());
        if (!check_err(err, "master start")) return;
        err = i2c_master_write_byte(cmd.get(), client, false);
        if (!check_err(err, "master write addr")) return;
        err = i2c_master_write(cmd.get(), bytes, len, false);
        if (!check_err(err, "master write")) return;
        err = i2c_master_stop(cmd.get());
        if (!check_err(err, "master stop")) return;
        err = i2c_master_cmd_begin(port, cmd.get(), 1000);
        if (!check_err(err, "master cmd begin")) return;
    }

    void read_bytes(uint8_t* buf, size_t len, const uint8_t* write, size_t write_len){
        unique_cmd_t cmd(i2c_cmd_link_create(), i2c_cmd_link_delete);
        esp_err_t err = i2c_master_start(cmd.get());
        if (!check_err(err, "master start")) return;
        err = i2c_master_write_byte(cmd.get(), client, true);
        if (!check_err(err, "master write addr")) return;
        if (write){
            err = i2c_master_write(cmd.get(), write, write_len, true);
            if (!check_err(err, "master write")) return;
        }
        err = i2c_master_write_byte(cmd.get(), client | I2C_MASTER_READ, true);
        if (!check_err(err, "master read addr")) return;
        err = i2c_master_read(cmd.get(), buf, len, I2C_MASTER_LAST_NACK);
        if (!check_err(err, "master read")) return;
        err = i2c_master_stop(cmd.get());
        if (!check_err(err, "master stop")) return;
        err = i2c_master_cmd_begin(port, cmd.get(), 1000);
        if (!check_err(err, "master cmd begin")) return;
    }

private:
    int port;
    uint8_t client = 0;
};
