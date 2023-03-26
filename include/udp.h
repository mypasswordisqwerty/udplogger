#ifndef UDP_H
#define UDP_H

#include <esp_event.h>
#include <esp_log.h>

#ifdef __cplusplus
extern "C" {
#endif

void udp_task(void*);
void udp_end();
void udp_start(esp_ip4_addr_t addr, uint16_t port);
void udp_send_bytes(const char* buf, int len);
void init_udp();

#ifdef __cplusplus
}
#endif

#endif //UDP_H