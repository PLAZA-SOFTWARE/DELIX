#include "arp.h"
#include "net.h"
#include "e1000.h"
#include "../terminal.h"
#include <stdint.h>

#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IP  0x0800

struct arp_entry {
    uint32_t ip; /* network byte order */
    uint8_t mac[6];
    int valid;
};

static struct arp_entry arp_table[16];

/* Ethernet + ARP packet structures (packed) */
struct eth_hdr { uint8_t dst[6]; uint8_t src[6]; uint16_t type; } __attribute__((packed));
struct arp_pkt {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint32_t spa;
    uint8_t tha[6];
    uint32_t tpa;
} __attribute__((packed));

/* Static transmit buffer */
static uint8_t txbuf[1600];

/* Local IP and MAC (set in net_init) */
extern uint32_t net_local_ip;
extern uint8_t net_local_mac[6];

/* Helpers */
static uint16_t htons(uint16_t v) { return (v<<8) | (v>>8); }
static uint32_t htonl(uint32_t v) { return ((uint32_t)htons(v&0xFFFF) << 16) | htons(v>>16); }

void arp_init(void) {
    for (int i = 0; i < (int)(sizeof(arp_table)/sizeof(arp_table[0])); i++) arp_table[i].valid = 0;
}

static void arp_table_add(uint32_t ip, const uint8_t mac[6]) {
    for (int i = 0; i < (int)(sizeof(arp_table)/sizeof(arp_table[0])); i++) {
        if (!arp_table[i].valid) {
            arp_table[i].ip = ip;
            for (int j=0;j<6;j++) arp_table[i].mac[j]=mac[j];
            arp_table[i].valid = 1;
            return;
        }
        if (arp_table[i].ip == ip) {
            for (int j=0;j<6;j++) arp_table[i].mac[j]=mac[j];
            arp_table[i].valid = 1;
            return;
        }
    }
}

static int arp_table_lookup(uint32_t ip, uint8_t out_mac[6]) {
    for (int i = 0; i < (int)(sizeof(arp_table)/sizeof(arp_table[0])); i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            for (int j=0;j<6;j++) out_mac[j]=arp_table[i].mac[j];
            return 1;
        }
    }
    return 0;
}

void arp_send_request(uint32_t target_ip) {
    struct eth_hdr* eth = (struct eth_hdr*)txbuf;
    for (int i=0;i<6;i++) eth->dst[i]=0xFF;
    for (int i=0;i<6;i++) eth->src[i]=net_local_mac[i];
    eth->type = htons(ETH_TYPE_ARP);

    struct arp_pkt* a = (struct arp_pkt*)(txbuf + sizeof(*eth));
    a->htype = htons(1);
    a->ptype = htons(ETH_TYPE_IP);
    a->hlen = 6;
    a->plen = 4;
    a->oper = htons(1); /* request */
    for (int i=0;i<6;i++) a->sha[i]=net_local_mac[i];
    a->spa = net_local_ip;
    for (int i=0;i<6;i++) a->tha[i]=0;
    a->tpa = target_ip;

    net_send_frame(txbuf, sizeof(*eth)+sizeof(*a));
}

void arp_receive(const uint8_t* buf, int len) {
    if (len < (int)(sizeof(struct eth_hdr)+sizeof(struct arp_pkt))) return;
    const struct eth_hdr* eth = (const struct eth_hdr*)buf;
    const struct arp_pkt* a = (const struct arp_pkt*)(buf + sizeof(*eth));
    if (a->ptype != htons(ETH_TYPE_IP)) return;
    uint16_t oper = (a->oper<<8) | (a->oper>>8);
    /* convert to host order */
    if (oper == 2) { /* reply */
        arp_table_add(a->spa, a->sha);
    } else if (oper == 1) { /* request */
        /* If request is for us, send reply */
        if (a->tpa == net_local_ip) {
            /* craft reply */
            struct eth_hdr* eth2 = (struct eth_hdr*)txbuf;
            for (int i=0;i<6;i++) eth2->dst[i]=eth->src[i];
            for (int i=0;i<6;i++) eth2->src[i]=net_local_mac[i];
            eth2->type = htons(ETH_TYPE_ARP);
            struct arp_pkt* r = (struct arp_pkt*)(txbuf + sizeof(*eth2));
            r->htype = htons(1);
            r->ptype = htons(ETH_TYPE_IP);
            r->hlen = 6; r->plen = 4;
            r->oper = htons(2);
            for (int i=0;i<6;i++) r->sha[i]=net_local_mac[i];
            r->spa = net_local_ip;
            for (int i=0;i<6;i++) r->tha[i]=a->sha[i];
            r->tpa = a->spa;
            net_send_frame(txbuf, sizeof(*eth2)+sizeof(*r));
        }
    }
}

int arp_resolve(uint32_t ip, uint8_t out_mac[6]) {
    if (arp_table_lookup(ip, out_mac)) return 1;
    /* send request and wait briefly */
    for (int i=0;i<3;i++) {
        arp_send_request(ip);
        /* simple wait: poll device multiple times */
        for (volatile int k=0;k<1000000;k++) {
            /* allow other code to run; in this freestanding kernel we rely on net_poll called elsewhere */
        }
        if (arp_table_lookup(ip, out_mac)) return 1;
    }
    return 0;
}
