#ifndef E1000_H
#define E1000_H

#include <stdint.h>

int e1000_init(void);
void e1000_poll(void);
int e1000_send(const uint8_t* buf, int len);
void e1000_get_mac(uint8_t mac[6]);

#endif
