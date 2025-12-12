#ifndef ARP_H
#define ARP_H

#include <stdint.h>

void arp_init(void);
void arp_receive(const uint8_t* buf, int len);
int arp_resolve(uint32_t ip, uint8_t out_mac[6]);

#endif
