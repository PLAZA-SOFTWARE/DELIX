#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <stdint.h>

int net_init(void);
void net_poll(void);
void net_receive(const unsigned char* buf, int len);
int net_ping(const char* host);

/* Low-level send provided by e1000
   (driver exposes a send function in e1000.c) */
int net_send_frame(const uint8_t* buf, int len);

#endif
