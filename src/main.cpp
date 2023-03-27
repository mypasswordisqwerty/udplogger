#include <esp_system.h>
#include "led.hpp"
#include "config.hpp"
#include "button.hpp"
#include "uart.hpp"
#include "screen.hpp"
#include "ina.hpp"
#include "udp.hpp"
#include "wifi.hpp"

QueueHandle_t Screen::queue = nullptr;
Messages UDP::msg;

void loop_forever(){
    while(true){
        vTaskDelay(portMAX_DELAY);
    }
}

void start_config_mode(Config& config){
    Uart uart(config, 0, Uart::MODE_CONFIG);
    uart.start(4096 * 4, configMAX_PRIORITIES-1);
    loop_forever();
}


void start_normal_mode(Config& config, Screen& screen){
    //screen.add_label(0, 8, 128, 0, "Normal mode");
    screen.add_label(0,0,42,0, "-.--V");
    screen.add_label(0,16,42,0, "-.--mV");
    screen.add_label(42,0,42,0, "-.--V");
    screen.add_label(42,16,42,0, "-.--mV");
    screen.add_label(84,0,42,0, "-.--V");
    screen.add_label(84,16,42,0, "-.--mV");
    screen.start(4096, 1);
    UDP udp(config);
    Uart uart1(config, 0);
    Uart uart2(config, 2);
    INA ina(config);
    udp.start(4096, configMAX_PRIORITIES-2);
    uart1.start(4096, configMAX_PRIORITIES-1);
    uart2.start(4096, configMAX_PRIORITIES-1);
    ina.start(4096, configMAX_PRIORITIES-3);
    WiFi wifi(config, udp);
    loop_forever();
}


extern "C" void app_main() {
    ESP_LOGI("MAIN", "Main started");
    Led led(static_cast<gpio_num_t>(CONFIG_USER_LED));
    Config config(led);
    Button btn(static_cast<gpio_num_t>(CONFIG_USER_BUTTON));
    Screen screen(config);
    //screen.add_label(0, 18, 128, 1, "Config mode");
    //screen.add_label(0, 32, 128, 2, "Config mode");
    if (!config.ready() || btn.is_long_pressed()){
        screen.add_label(0, 8, 128, 0, "Config mode");
        start_config_mode(config);
    }else{
        start_normal_mode(config, screen);
    }
}