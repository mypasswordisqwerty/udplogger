#include "button.hpp"
#include "config.hpp"
#include "ina.hpp"
#include "led.hpp"
#include "screen.hpp"
#include "uart.hpp"
#include "udp.hpp"
#include "wifi.hpp"
#include <esp_system.h>

QueueHandle_t Screen::queue = nullptr;
Messages UDP::msg;

void loop_forever()
{
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

void start_config_mode(Config& config)
{
    Uart uart(config, 0, Uart::MODE_CONFIG);
    uart.start(4096 * 4, configMAX_PRIORITIES - 1);
    loop_forever();
}

void start_normal_mode(Config& config, Screen& screen)
{
    screen.add_label(0, 0, 42, 0, "-.--V");
    screen.add_label(0, 12, 42, 0, "-.--mA");
    screen.add_label(42, 0, 42, 0, "-.--V");
    screen.add_label(42, 12, 42, 0, "-.--mA");
    screen.add_label(84, 0, 42, 0, "-.--V");
    screen.add_label(84, 12, 42, 0, "-.--mA");
    screen.add_label(0, 24, 128, 0, "---.---.---.---");
    screen.start(4096 * 2, 1);
    UDP udp(config);
    Uart uart1(config, 1);
    Uart uart2(config, 2);
    INA ina(config);
    udp.start(4096 * 10, configMAX_PRIORITIES - 2);
    uart1.start(4096 * 2, configMAX_PRIORITIES - 1);
    uart2.start(4096 * 2, configMAX_PRIORITIES - 1);
    ina.start(4096 * 2, configMAX_PRIORITIES - 3);
    WiFi wifi(config, udp);
    wifi.start(4096, 1);
    loop_forever();
}

extern "C" void app_main()
{
    ESP_LOGI("MAIN", "Main started");
    Led led(static_cast<gpio_num_t>(CONFIG_USER_LED));
    Config config(led);
    Button btn(static_cast<gpio_num_t>(CONFIG_USER_BUTTON));
    Screen screen(config);
    if (!config.ready() || btn.is_long_pressed()) {
        screen.add_label(0, 8, 128, 0, "Config mode");
        start_config_mode(config);
    } else {
        start_normal_mode(config, screen);
    }
}