#pragma once

#include "common.h"
#include <esp_event.h>


#define QSIZE 100

class Messages:public Base{
public:
    Messages(): Base("Messages"){
        queue = xQueueCreate(QSIZE, sizeof(int));
        for (int i=0; i < QSIZE; i++){
            mutexes[i] = xSemaphoreCreateMutex();
            if (!mutexes[i]) {
                ESP_LOGE(TAG, "Mutex for message %d not created!!!", i);
            }
        }
    }

    void add_message(const std::string& str){
        int id = currentMessage++;
        xSemaphoreTake(mutexes[id], portMAX_DELAY);
        msgs[id] = str;
        xSemaphoreGive(mutexes[id]);
        if (xQueueSendToBack(queue, &id, ( TickType_t ) 100) != pdTRUE){
            ESP_LOGE(TAG, "Send queue full");
        }
        if (currentMessage >= QSIZE){
            currentMessage = 0;
        }
    }

    bool get_message(std::string& message){
        int id;
        if (xQueueReceive(queue, &id, 0) != pdTRUE) return false;
        xSemaphoreTake(mutexes[id], portMAX_DELAY);
        message = msgs[id];
        xSemaphoreGive(mutexes[id]);
        return true;
    }

    void clear(){
        int id;
        while(xQueueReceive(queue, &id, 0) == pdTRUE);
    }


private:
    int currentMessage = 0;
    std::string msgs[QSIZE];
    QueueHandle_t queue;
    SemaphoreHandle_t mutexes[QSIZE];
};