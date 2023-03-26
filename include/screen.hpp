#pragma once

#include "common.h"
#include "i2c.hpp"
#include "config.hpp"
#include <fonts.h>
#include <map>

#define BUF_SIZE 128 * 8 + 1

class Screen: public I2C{
public:
    Screen(Config& config, bool updown=false): I2C("Screen", 0), updown(updown),
        fonts{&Font_7x10, &Font_11x18, &Font_16x26}
    {
        if (!config.ready()){
            return;
        }
        uint16_t pins = config.screen_i2c();
        ESP_LOGI(TAG, "Init screen to pins scl %d sda %d", pins & 0xFF, (pins >> 8));
        if (!init_i2c(pins & 0xFF, (pins >> 8) & 0xFF, 0x3C)) {
            ESP_LOGE(TAG, "Screen init failed");
            return;
        }
        vTaskDelay(100);
        buf = &buf_data[1];
        buf_data[0] = 0x40;
        inited = true;
        init_cmds();
        if (!queue){
            queue = xQueueCreate(10, sizeof(ScreenUpdate));
        }
        ESP_LOGI(TAG, "Screen inited");
    }


    int add_label(int x, int y, int w, int font_id, std::string text){
        Label l{x, y, w, fonts[font_id], text};
        labels.push_back(l);
        int ret =  labels.size() - 1;
        draw_label(ret);
        send_screen();
        return ret;
    }

private:
    struct Label{
        int x, y, w;
        FontDef_t* font;
        std::string text;
    };
    struct ScreenUpdate{
        int id;
        char text[20];
    };

public:

    static void update_label(int id, std::string text){
        if (!queue) return;
        text.resize(19);
        ScreenUpdate upd;
        upd.id = id;
        memcpy(upd.text, text.c_str(), 20);
        xQueueSend(queue, &upd, 100);
    }

    void run(){
        while(true){
            ScreenUpdate upd;
            std::map<int, std::string> texts;
            while(xQueueReceive(queue, &upd, 10)){
                texts[upd.id] = std::string(upd.text);
            }
            if (texts.size()>0){
                update_labels(texts);
            }
            vTaskDelay(100);
        }
    }



private:

    void init_cmds(){
        send_cmd(0xAE);
        send_cmd(0x20);
        send_cmd(0x00);

        send_cmd(updown ? 0xC8 : 0xC0);
        send_cmd(0xDA);
        send_cmd(updown ? 0x32 : 0x02);

        send_cmd(0x8D); //--set DC-DC enable
        send_cmd(0x14); //

        send_cmd(0xAF);
        send_screen();
    }

    void send_cmd(uint8_t cmd){
        if (!inited){
            return;
        }
        uint16_t _cmd = cmd << 8;
        write_bytes((uint8_t*)&_cmd, 2);
    }

    void send_screen(){
        if (!inited){
            return;
        }
        write_bytes(buf_data, BUF_SIZE);
    }

    const uint16_t* get_letter(Label& l, int pos){
        if (l.text.length() <= pos){
            return l.font->data;
        }
        char c = l.text[pos];
        int ofs = ((int)c - 0x20) * l.font->FontHeight;
        return &l.font->data[ofs];
    }

    void draw_label_col(Label& l, const uint16_t* letter, int font_bit, int x){
        int y = updown ? 63 - l.y : l.y;
        uint8_t* cb = &buf[128 * (y/8) + x];
        int bbit = y % 8;
        for (int i = 0; i < l.font->FontHeight; i++){
            if (updown){
                if (y-- < 0) return;
            }else{
                if (y++ >= 63) return;
            }
            uint8_t bit = (1 << bbit++);
            if ((letter[i] >> font_bit) & 1){
                // set screen bit
                *cb |= bit;
            }else{
                // clear screen bit
                *cb &= ~bit;
            }
            if (y % 8 == 0){
                cb = &buf[128 * (y/8) + x]; 
                bbit = 0;
            }
        }
    }

    void draw_label(int id){
        Label& l = labels[id];
        int lpos = 0;
        const uint16_t* letter = nullptr;
        int font_bit = 0;

        int x = l.x;
        for(int i = 0; i < l.w; i++){
            if (x >= 128) return;
            if (i % l.font->FontWidth == 0){
                letter = get_letter(l, lpos++);
                font_bit = 15;
            }
            draw_label_col(l, letter, font_bit--, x++);
        }        
    }

    void update_labels(std::map<int, std::string> texts){
        for (auto& val: texts){
            labels[val.first].text = val.second;
            draw_label(val.first);
        }
        send_screen();
    }

private:
    bool updown;
    uint8_t buf_data[BUF_SIZE] = {0};
    uint8_t* buf;
    bool inited = false;
    std::vector<Screen::Label> labels;
    FontDef_t* fonts[3];
    static QueueHandle_t queue;
};
