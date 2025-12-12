/* net.c - removed network support; provide stubs */

#include "net.h"

int net_init(void) { return -1; }
void net_poll(void) { }
void net_receive(const unsigned char* buf, int len) { (void)buf; (void)len; }
int net_ping(const char* host) { (void)host; return -1; }
int net_send_frame(const uint8_t* buf, int len) { (void)buf; (void)len; return -1; }
