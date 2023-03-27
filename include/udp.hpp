#ifndef UDP_H
#define UDP_H

#include "common.h"
#include "config.hpp"
#include "messages.hpp"
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <sstream>

class UDP: public Thread{
public:
    UDP(Config& config):Thread("UDP"){
        port = config.port();
    }

    void run(){
        std::string message;
        while(true){
            if (!netready){
                ESP_LOGI(TAG, "Net not ready");
                close_udp();
                msg.clear();
                vTaskDelay(1000);
                continue;
            }
            if (_socket < 0 && !createSocket()){
                ESP_LOGI(TAG, "No socket");
                msg.clear();
                vTaskDelay(100);
                continue;
            }
            processUDPCommands();
            while(msg.get_message(message)){
                if (!remote_addr) continue;
                sendUdp(message, remote_addr);
            }
        }
    }

    void net_start(esp_ip4_addr_t address){
        addr = address;
        netready = true;
    }

    void net_end(){
        netready = false;
    }

    static void send(const std::string& message){
        msg.add_message(message);
    }

private:
    bool createSocket(){
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(port);
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            return false;
        }
        ESP_LOGD(TAG, "Socket created");
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        int err = bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
        if (err < 0) {
            close(sock);
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            return false;
        }
        ESP_LOGI(TAG, "Socket bound, port %d", port);
        _socket = sock;
        return true;
    }

    void close_udp(){
        remote_addr = nullptr;
        if (_socket < 0) return;
        ::shutdown(_socket, 0);
        ::close(_socket);
        _socket = -1;
    }

    bool sendUdp(const std::string &msg, struct sockaddr_storage *addr){
        char addr_str[128];
        inet_ntoa_r(((struct sockaddr_in *)addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        uint16_t port = ntohs( ((struct sockaddr_in *)addr)->sin_port );
        ESP_LOGD(TAG, "Send udp data %s to %s:%d", msg.c_str(), addr_str, port);
        int err = ::sendto(_socket, msg.c_str(), msg.length(), 0, (struct sockaddr *)addr, sizeof(struct sockaddr_storage));
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            return false;
        }
        return true;
    }

    std::string receiveCommand(struct sockaddr_storage *source_addr){
        char buf[512];
        socklen_t socklen = sizeof(struct sockaddr_storage);
        int len = recvfrom(_socket, buf, 511, 0, (struct sockaddr *)source_addr, &socklen);
        if (len == 0) return "";
        if (len < 0) {
            if (errno != EAGAIN){
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            }
            return "";
        }
        std::string cmd(buf, len);
        if(cmd.substr(0,4) != "UUL "){
            ESP_LOGE(TAG, "Received unsupported message format");
            return "";
        }
        return cmd;
    }

    void processUDPCommands(){
        char addr_str[128];
        struct sockaddr_storage source_addr;
        std::string cmd = receiveCommand(&source_addr);
        if (cmd.empty()){
            return;
        }
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "Command received %s from %s", cmd.c_str(), addr_str);
        std::stringstream ss(cmd);
        std::string s;
        std::getline(ss, s, ' ');
        std::getline(ss, cmd, ' ');
        ESP_LOGI(TAG, "Command is %s", cmd.c_str());
        vTaskDelay(100);
        if (cmd == "PING"){
            sendUdp("UUL PONG", &source_addr);
        }else if (cmd == "STOP"){
            remote_addr = nullptr;
            sendUdp("UUL OK", &source_addr);
        }else if (cmd == "START"){
            uint16_t rport = -1;
            if (std::getline(ss, s, ' ')){
                rport = (uint16_t)std::stoi(s);
            }
            port = rport;
            receiver = source_addr;
            remote_addr = &receiver;
            sendUdp("UUL OK", remote_addr);
        }else{
            sendUdp("UUL ERR UNSUPPORTED COMMAND", &source_addr);
        }
    }

private:
    uint16_t port;
    esp_ip4_addr_t addr;
    static Messages msg;
    volatile bool netready = false;
    volatile int _socket = -1;
    struct sockaddr_storage receiver;
    struct sockaddr_storage *remote_addr = nullptr;
};

#endif //UDP_H