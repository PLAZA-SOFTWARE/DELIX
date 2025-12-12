#include "arp.h"

void arp_init(void) { }
void arp_receive(const uint8_t* buf, int len) { (void)buf; (void)len; }
int arp_resolve(uint32_t ip, uint8_t out_mac[6]) { (void)ip; (void)out_mac; return 0; }
