#pragma once

#include "common.h"
#include "config.hpp"
#include "udp.hpp"
#include <driver/uart.h>
#include <sstream>
#include <string>

#define CFG_BUF 2048

typedef std::vector<std::string> strings;

class Uart : public Thread {
public:
    enum Mode {
        MODE_NORMAL = 0,
        MODE_CONFIG = 1
    };

public:
    Uart(Config& config, uart_port_t port, Mode mode = MODE_NORMAL)
        : Thread("UART" + std::to_string(port))
        , config(config)
        , port(port)
        , mode(mode)
    {
        ESP_LOGD(TAG, "Init uart%d on pin %d", port, config.get_uart_io(port));
        const uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_APB,
        };

        if (mode == MODE_CONFIG) {
            ESP_ERROR_CHECK(uart_driver_install(port, CFG_BUF, CFG_BUF, 0, NULL, 0));
            ESP_ERROR_CHECK(uart_param_config(port, &uart_config));
            ESP_ERROR_CHECK(uart_set_pin(port, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

        } else {
            // We won't use a buffer for sending data.
            ESP_ERROR_CHECK(uart_driver_install(port, 500 * 2, 0, 0, NULL, 0));
            ESP_ERROR_CHECK(uart_param_config(port, &uart_config));
            ESP_ERROR_CHECK(uart_set_pin(port, UART_PIN_NO_CHANGE, config.get_uart_io(port), UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        }
        ESP_LOGI(TAG, "uart%d enabled in mode %d on pin %d", port, mode, config.get_uart_io(port));
    }

private:
    void normal_run()
    {
        char buf[500];
        while (true) {
            int rxBytes = uart_read_bytes(port, buf, 500, 50 / portTICK_RATE_MS);
            if (rxBytes < 0) {
                ESP_LOGE(TAG, "UART read error: %d", rxBytes);
            }
            if (rxBytes > 0) {
                ESP_LOGI(TAG, "UART %d read %d bytes", port, rxBytes);
                buf[rxBytes] = 0;
                std::string msg(buf);
                UDP::send(std::to_string(port) + ": " + msg);
                // some indication
                config.led().toggle();
            }
        }
    }

    strings process_config(const strings& cmd)
    {
        strings ret;
        if (cmd[0] == "getconfig") {
            ret.push_back("config");
            ret.push_back(config.ssid());
            ret.push_back(std::to_string(config.port()));
            ret.push_back(std::to_string(config.get_uart()));
            ret.push_back(std::to_string(config.screen_i2c()));
            ret.push_back(std::to_string(config.ina_i2c()));
        } else if (cmd[0] == "setconfig") {
            if (cmd.size() != 7) {
                ret.push_back("error");
                ret.push_back("wrong config");
            } else {
                config.set_config_wifi(cmd[1], cmd[2], std::stoi(cmd[3]));
                config.set_config_ports(std::stoi(cmd[4]), std::stoi(cmd[5]), std::stoi(cmd[6]));
                ret.push_back("ok");
            }
        } else if (cmd[0] == "save") {
            config.save_config();
            ret.push_back("ok");
        } else if (cmd[0] == "ping") {
            ret.push_back("pong");
        } else {
            ret.push_back("error");
            ret.push_back("Unknown command " + cmd[0]);
        }
        return ret;
    }

    std::string join_string(const strings& strs)
    {
        std::string ret;
        for (auto& s : strs) {
            if (ret.length() != 0)
                ret += ":";
            ret += s;
        }
        return ret;
    }

    strings split_string(const std::string& str)
    {
        strings ret;
        std::string word;
        std::stringstream ss(str);
        std::string trim = " :\r\n\t";
        while (std::getline(ss, word, ':')) {
            while (trim.find(word[0]) != std::string::npos)
                word = word.substr(1);
            while (trim.find(word[word.length() - 1]) != std::string::npos)
                word = word.substr(0, word.length() - 2);
            ret.push_back(word);
        }
        return ret;
    }

    std::string build_config_cmd(const std::string& arr)
    {
        cmd += arr;
        std::string ret;
        size_t pos = cmd.find("\n");
        if (pos == std::string::npos) {
            return ret;
        }
        ret = cmd.substr(0, pos);
        cmd = cmd.substr(pos + 1);
        return ret;
    }

    void config_run()
    {
        char buf[CFG_BUF];
        while (true) {
            int rxBytes = uart_read_bytes(port, buf, CFG_BUF, 1000 / portTICK_RATE_MS);
            if (rxBytes < 0) {
                ESP_LOGE(TAG, "UART read error: %d", rxBytes);
                continue;
            }
            if (rxBytes > 0) {
                in_config = true;
            }
            if (!in_config) {
                uart_write_bytes(port, "Config mode\n", 12);
                config.led().toggle();
            } else {
                std::string cmd = build_config_cmd(std::string(buf, rxBytes));
                if (!cmd.empty()) {
                    strings resp = process_config(split_string(cmd));
                    std::string ans = join_string(resp) + "\n";
                    uart_write_bytes(port, ans.c_str(), ans.length());
                }
            }
        }
    }

    virtual void run() override
    {
        if (mode == MODE_CONFIG) {
            config_run();
        } else {
            normal_run();
        }
    }

private:
    Config& config;
    uart_port_t port;
    Mode mode;
    bool in_config = false;
    std::string cmd;
};
