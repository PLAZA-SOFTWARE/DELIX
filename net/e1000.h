#ifndef E1000_H
#define E1000_H

#include <stdint.h>

int e1000_init(void);
void e1000_poll(void);

/* Send a raw Ethernet frame (buf must remain valid or point into static memory) */
int e1000_send(const uint8_t* buf, int len);

/* Get device MAC address */
void e1000_get_mac(uint8_t mac[6]);

#endif
