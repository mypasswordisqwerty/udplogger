#pragma once

#include <esp_system.h>
#include <esp_log.h>

#include <string>
#include <vector>

#define TAG (name.c_str())

typedef std::vector<uint8_t> byte_array;

class Base{
public:
    Base(std::string name): name(name){}
protected:
    std::string name;
};

class Thread: public Base{
public:
    Thread(std::string name): Base(name){}
    virtual ~Thread(){}

    void start(uint32_t stack, UBaseType_t priority){
        xTaskCreate(&Thread::_run, name.c_str(), stack, this, priority, NULL);
    }

protected:
    virtual void run() = 0;

    inline void delay(TickType_t ms){
        vTaskDelay(ms / portTICK_RATE_MS);
    }

private:
    static void _run(void* thiz){
        ((Thread*)thiz)->run();
    }

};
