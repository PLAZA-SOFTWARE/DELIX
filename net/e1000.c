/* e1000.c - removed network support; provide stubs to satisfy linking */

#include "e1000.h"

int e1000_init(void) { return -1; }
void e1000_poll(void) { }
int e1000_send(const uint8_t* buf, int len) { (void)buf; (void)len; return -1; }
void e1000_get_mac(uint8_t mac[6]) { (void)mac; }
