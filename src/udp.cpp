#include <udp.h>
#include <freertos/FreeRTOS.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <esp_system.h>

#include <string>
#include <sstream>

#define QSIZE 10
static const char *TAG = "UDP";


struct Message {
    std::string msg;
    SemaphoreHandle_t mutex;

    void set(const char* buf, int len){
        xSemaphoreTake(mutex, portMAX_DELAY);
        msg = std::string(buf, len);
        xSemaphoreGive(mutex);
    }
    std::string get(){
        xSemaphoreTake(mutex, portMAX_DELAY);
        std::string ret = msg;
        xSemaphoreGive(mutex);
        return ret;
    }
};

struct UDP {
    esp_ip4_addr_t addr;
    uint16_t port;
    volatile int socket = -1;
    struct sockaddr_storage receiver;
    uint16_t remote_port;
    struct sockaddr_storage *remote_addr = nullptr;

    void close(){
        remote_addr = nullptr;
        if (socket < 0) return;
        ::shutdown(socket, 0);
        ::close(socket);
        socket = -1;
    }
};

static QueueHandle_t queue;
static Message messages[QSIZE];
static int currentMessage = 0;
static volatile bool net_ready = false;
static UDP udp;


static bool createSocket() {
    struct sockaddr_in bind_addr;
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(udp.port);
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
    ESP_LOGI(TAG, "Socket bound, port %d", udp.port);
    udp.socket = sock;
    return true;
}

static std::string receiveCommand(struct sockaddr_storage *source_addr){
    char buf[512];
    socklen_t socklen = sizeof(struct sockaddr_storage);
    int len = recvfrom(udp.socket, buf, 511, 0, (struct sockaddr *)source_addr, &socklen);
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

static bool sendUdp(const std::string &msg, struct sockaddr_storage *addr){
    char addr_str[128];
    inet_ntoa_r(((struct sockaddr_in *)addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
    uint16_t port = ntohs( ((struct sockaddr_in *)addr)->sin_port );
    ESP_LOGD(TAG, "Send udp data %s to %s:%d", msg.c_str(), addr_str, port);
    int err = ::sendto(udp.socket, msg.c_str(), msg.length(), 0, (struct sockaddr *)addr, sizeof(struct sockaddr_storage));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return false;
    }
    return true;
}

static void processUDPServer() {
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
        udp.remote_addr = nullptr;
        sendUdp("UUL OK", &source_addr);
    }else if (cmd == "START"){
        uint16_t port = -1;
        if (std::getline(ss, s, ' ')){
            port = (uint16_t)std::stoi(s);
        }
        udp.port = port;
        udp.receiver = source_addr;
        udp.remote_addr = &udp.receiver;
        sendUdp("UUL OK", udp.remote_addr);
    }else{
        sendUdp("UUL ERR UNSUPPORTED COMMAND", &source_addr);
    }
}

void udp_task(void*){
    int val;
    while(true){
        if (!net_ready){
            ESP_LOGI(TAG, "Net not ready");
            udp.close();
            while(xQueueReceive(queue, &val, 0) == pdTRUE);
            vTaskDelay(100);
            continue;
        }
        if (udp.socket < 0){
            ESP_LOGI(TAG, "No socket");
            if (!createSocket()) {
                while(xQueueReceive(queue, &val, 0) == pdTRUE);
                vTaskDelay(100);
                continue;
            }
        }
        while(xQueueReceive(queue, &val, 0) == pdTRUE) {
            ESP_LOGD(TAG, "Queue received %d addr %8.8X", val, (uint32_t)udp.remote_addr);
            if (!udp.remote_addr) continue;
            sendUdp(messages[val].get(), udp.remote_addr);
        }
        processUDPServer();
    }
}

void udp_end(){
    net_ready = false;
}

void udp_start(esp_ip4_addr_t addr, uint16_t port){
    udp.addr = addr;
    udp.port = port;
    net_ready = true;
}

void udp_send_bytes(const char* buf, int len){
    int msgid = currentMessage++;
    if (currentMessage >= QSIZE){
        currentMessage = 0;
    }
    messages[msgid].set(buf, len);
    if (xQueueSendToBack(queue, &msgid, ( TickType_t ) 100) != pdTRUE){
        ESP_LOGE(TAG, "Send queue full");
    }
}

void init_udp(){
    queue = xQueueCreate(QSIZE, sizeof(int));
    for (int i=0; i<QSIZE; i++){
        messages[i].mutex = xSemaphoreCreateMutex();
        if (!messages[i].mutex){
            ESP_LOGE(TAG, "Mutex for message %d not created!!!", i);
        }
    }
}
