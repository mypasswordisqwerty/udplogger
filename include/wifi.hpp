#pragma once

#include "common.h"
#include "config.hpp"
#include "udp.hpp"
#include <esp_wifi.h>
#include <freertos/event_groups.h>

class WiFi: public Thread{

public:
    WiFi(Config& config, UDP& udp): Thread("WiFi"), config(config), udp(udp){
        s_wifi_event_group = xEventGroupCreate();
    }

    void run(){
        ESP_LOGI(TAG, "init  wifi");
        init_wifi(config);
        ESP_LOGI(TAG, "after init wifi");

        /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
        * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                BIT0 | BIT1,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

        /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
        * happened. */
        if (bits & BIT0) {
            ESP_LOGI(TAG, "connected to ap SSID: %s", config.ssid().c_str());
        } else if (bits & BIT1) {
            ESP_LOGI(TAG, "Failed to connect to SSID: %s", config.ssid().c_str());
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
        while(true){
            vTaskDelay(portMAX_DELAY);
        }
    }

private:
    void init_wifi(Config& config)
    {
        s_wifi_event_group = xEventGroupCreate();

        ESP_ERROR_CHECK(esp_netif_init());

        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &WiFi::event_handler,
                                                            this,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &WiFi::event_handler,
                                                            this,
                                                            &instance_got_ip));

        wifi_config_t wifi_config;
        memset(&wifi_config, 0, sizeof(wifi_config_t));
        std::string& ssid = config.ssid();
        memcpy(wifi_config.sta.ssid, ssid.c_str(), ssid.length()+1); 
        std::string& pswd = config.pswd();
        memcpy(wifi_config.sta.password, pswd.c_str(), pswd.length()+1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );

        ESP_LOGI(TAG, "wifi_init_sta finished.");
    }

    void on_event(esp_event_base_t event_base, int32_t event_id, void* event_data){
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            udp.net_end();
            if (s_retry_num < 100) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG, "retry to connect to the AP");
            } else {
                xEventGroupSetBits(s_wifi_event_group, BIT1);
            }
            ESP_LOGE(TAG,"connect to the AP fail");
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, BIT0);
            udp.net_start(event->ip_info.ip);
        }
    }

    static void event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
    {
        ((WiFi*)arg)->on_event(event_base, event_id, event_data);
    }

private:
    Config& config;
    UDP& udp;
    int s_retry_num = 0;
    EventGroupHandle_t s_wifi_event_group;
};
