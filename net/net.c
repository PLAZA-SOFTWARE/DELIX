/* net.c - simple network subsystem that integrates e1000 driver, ARP, IP, ICMP basic support */

#include "net.h"
#include "e1000.h"
#include "../terminal.h"
#include "arp.h"

static int net_initialized = 0;
uint32_t net_local_ip = 0; /* network byte order */
uint8_t net_local_mac[6] = {0,0,0,0,0,0};

/* simple transmit buffer used by ARP/ICMP */
static uint8_t txbuf[1600];

/* ping state */
static volatile int ping_waiting = 0;
static volatile int ping_received = 0;
static uint16_t ping_id = 0x1234;
static uint16_t ping_seq = 0;

static uint16_t ip_checksum(const void* vdata, int length) {
    const uint8_t* data = (const uint8_t*)vdata;
    uint32_t sum = 0;
    for (int i = 0; i + 1 < length; i += 2) {
        sum += (uint16_t)(data[i] << 8 | data[i+1]);
    }
    if (length & 1) {
        sum += (uint16_t)(data[length-1] << 8);
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* parse dotted IPv4, return 1 on success and set out (network byte order) */
static int parse_ipv4(const char* s, uint32_t* out) {
    int parts[4] = {0,0,0,0};
    int idx = 0;
    int val = 0;
    int seen = 0;
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            val = val * 10 + (*s - '0');
            if (val > 255) return 0;
            seen = 1;
        } else if (*s == '.') {
            if (!seen || idx >= 3) return 0;
            parts[idx++] = val;
            val = 0;
            seen = 0;
        } else {
            return 0;
        }
    }
    if (!seen || idx != 3) return 0;
    parts[3] = val;
    *out = ((uint32_t)parts[0] << 24) | ((uint32_t)parts[1] << 16) | ((uint32_t)parts[2] << 8) | ((uint32_t)parts[3]);
    return 1;
}

int net_init(void) {
    if (e1000_init() != 0) return -1;
    /* Initialize ARP/IP stacks here */
    /* set local MAC from driver */
    e1000_get_mac(net_local_mac);
    arp_init();
    /* Set local IP for demo (use QEMU user-net default 10.0.2.15) */
    net_local_ip = 0x0A00020F; /* 10.0.2.15 in network byte order bytes */
    net_initialized = 1;
    return 0;
}

void net_poll(void) {
    if (!net_initialized) return;
    e1000_poll();
    /* Poll ARP/IP/ICMP etc (handled in net_receive) */
}

void net_receive(const unsigned char* buf, int len) {
    /* Called by driver when a raw Ethernet frame arrives */
    print("net: frame received (len=");
    /* simple decimal print */
    char tmp[12];
    int n = len;
    int pos = 0;
    if (n == 0) tmp[pos++] = '0';
    else {
        int div = 1000000000;
        int started = 0;
        for (int i = 0; i < 10; i++) {
            int d = n / div;
            if (d || started) {
                tmp[pos++] = '0' + d;
                started = 1;
            }
            n %= div;
            div /= 10;
        }
    }
    tmp[pos] = '\0';
    print(tmp);
    print(")\n");

    /* dispatch to ARP handler if needed */
    if (len >= 14) {
        uint16_t et = ((uint16_t)buf[12] << 8) | buf[13];
        if (et == 0x0806) { /* ARP */
            arp_receive(buf, len);
            return;
        }
        if (et == 0x0800 && len >= 14 + 20) { /* IPv4 */
            const uint8_t* ip = buf + 14;
            uint8_t ihl = (ip[0] & 0x0F) * 4;
            if (len < 14 + ihl + 8) return; /* not enough for ICMP */
            uint8_t proto = ip[9];
            if (proto == 1) { /* ICMP */
                const uint8_t* icmp = ip + ihl;
                uint8_t type = icmp[0];
                uint8_t code = icmp[1];
                uint16_t rcv_id = (icmp[4] << 8) | icmp[5];
                uint16_t rcv_seq = (icmp[6] << 8) | icmp[7];
                if (type == 0 && code == 0) { /* Echo reply */
                    if (ping_waiting && rcv_id == ping_id) {
                        ping_received = 1;
                    }
                }
            }
        }
    }
}

int net_ping(const char* host) {
    if (!net_initialized) return -1;
    uint32_t dst_ip;
    if (!parse_ipv4(host, &dst_ip)) {
        print("net_ping: only dotted IPv4 supported\n");
        return -1;
    }

    /* Resolve via ARP */
    uint8_t dst_mac[6];
    if (!arp_resolve(dst_ip, dst_mac)) {
        print("net_ping: ARP resolution failed\n");
        return -1;
    }

    /* Build Ethernet header */
    struct eth_hdr { uint8_t dst[6]; uint8_t src[6]; uint16_t type; } __attribute__((packed));
    struct ip_hdr {
        uint8_t vhl;
        uint8_t tos;
        uint16_t tot_len;
        uint16_t id;
        uint16_t frag_off;
        uint8_t ttl;
        uint8_t proto;
        uint16_t checksum;
        uint32_t saddr;
        uint32_t daddr;
    } __attribute__((packed));
    struct icmp_hdr {
        uint8_t type;
        uint8_t code;
        uint16_t checksum;
        uint16_t id;
        uint16_t seq;
    } __attribute__((packed));

    int ip_header_len = sizeof(struct ip_hdr);
    int icmp_payload = 32;
    int icmp_len = sizeof(struct icmp_hdr) + icmp_payload;
    int ip_tot_len = ip_header_len + icmp_len;
    int eth_len = sizeof(struct eth_hdr) + ip_tot_len;

    if (eth_len > (int)sizeof(txbuf)) return -1;

    struct eth_hdr* eth = (struct eth_hdr*)txbuf;
    for (int i=0;i<6;i++) eth->dst[i]=dst_mac[i];
    for (int i=0;i<6;i++) eth->src[i]=net_local_mac[i];
    eth->type = (uint16_t)((0x0800 >> 8) | ((0x0800 & 0xFF) << 8)); /* htons(0x0800) */
    
    struct ip_hdr* iph = (struct ip_hdr*)(txbuf + sizeof(*eth));
    iph->vhl = (4 << 4) | (ip_header_len/4);
    iph->tos = 0;
    iph->tot_len = (uint16_t)((ip_tot_len >> 8) | (ip_tot_len << 8)); /* htons */
    iph->id = 0;
    iph->frag_off = 0;
    iph->ttl = 64;
    iph->proto = 1; /* ICMP */
    iph->checksum = 0;
    iph->saddr = net_local_ip;
    iph->daddr = dst_ip;
    iph->checksum = ip_checksum(iph, ip_header_len);

    struct icmp_hdr* icmp = (struct icmp_hdr*)(txbuf + sizeof(*eth) + ip_header_len);
    icmp->type = 8; /* Echo request */
    icmp->code = 0;
    icmp->id = (ping_id >> 8) | (ping_id << 8);
    icmp->seq = (ping_seq >> 8) | (ping_seq << 8);
    /* fill payload */
    uint8_t* payload = (uint8_t*)icmp + sizeof(*icmp);
    for (int i=0;i<icmp_payload;i++) payload[i] = (uint8_t)i;
    icmp->checksum = 0;
    icmp->checksum = ip_checksum(icmp, icmp_len);

    /* send frame */
    ping_waiting = 1;
    ping_received = 0;
    if (net_send_frame(txbuf, sizeof(*eth) + ip_tot_len) != 0) {
        print("net_ping: failed to send frame\n");
        ping_waiting = 0;
        return -1;
    }

    /* wait for reply (poll) */
    for (int i=0; i<2000000; i++) {
        net_poll();
        if (ping_received) break;
    }
    ping_waiting = 0;
    if (ping_received) {
        print("net_ping: reply received\n");
        ping_seq++;
        return 0;
    }
    print("net_ping: timeout\n");
    ping_seq++;
    return -1;
}

int net_send_frame(const uint8_t* buf, int len) {
    return e1000_send(buf, len);
}
