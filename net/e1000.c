/* e1000 driver: PCI discovery, MMIO BAR mapping via config space I/O, basic Rx/Tx descriptor setup (polling)
   Simplified for a 32-bit freestanding demo. Assumes IO access to PCI config and identity of device.
*/

#include "e1000.h"
#include "../terminal.h"
#include "net.h"
#include <stdint.h>

/* PCI config ports */
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* e1000 PCI IDs (vendor/device) */
#define INTEL_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E /* common in QEMU e1000 */

/* e1000 registers offsets (MMIO) */
#define REG_CTRL  0x0000
#define REG_STATUS 0x0008
#define REG_RCTL  0x0100
#define REG_TCTL  0x0400
#define REG_RDBAL 0x2800
#define REG_RDBAH 0x2804
#define REG_RDLEN 0x2808
#define REG_RDH   0x2810
#define REG_RDT   0x2818
#define REG_TDBAL 0x3800
#define REG_TDBAH 0x3804
#define REG_TDLEN 0x3808
#define REG_TDH   0x3810
#define REG_TDT   0x3818
#define REG_MAC_LOW  0x5400
#define REG_MAC_HIGH 0x5404

/* Descriptor counts */
#define RX_DESC_COUNT 32
#define TX_DESC_COUNT 8

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t csum_offset;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

/* Simple memory areas (statically allocated) */
static uint8_t rx_buffers[RX_DESC_COUNT][2048];
static struct e1000_rx_desc rx_descs[RX_DESC_COUNT];
static struct e1000_tx_desc tx_descs[TX_DESC_COUNT];

/* MMIO base found via PCI */
static volatile uint8_t* mmio_base = (volatile uint8_t*)0;
static int initialized = 0;
static uint32_t rx_tail = 0;
static uint32_t tx_tail = 0;
static uint8_t mac_addr[6];

/* Discovered PCI location */
static uint8_t found_bus = 0xFF;
static uint8_t found_slot = 0xFF;

/* I/O port access (provided by kernel) */
extern void outl(unsigned short port, unsigned int val);
extern unsigned int inl(unsigned short port);
extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);

/* PCI helper to read config dword */
static uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t id = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, id);
    return inl(PCI_CONFIG_DATA);
}

static void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t id = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, id);
    outl(PCI_CONFIG_DATA, value);
}

/* Find e1000 device and return BAR0 */
static uint32_t find_e1000_bar0(void) {
    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t v = pci_config_read_dword(bus, slot, 0, 0);
            uint16_t vendor = v & 0xFFFF;
            uint16_t device = (v >> 16) & 0xFFFF;
            if (vendor == INTEL_VENDOR_ID && device == E1000_DEVICE_ID) {
                /* read BAR0 */
                uint32_t bar0 = pci_config_read_dword(bus, slot, 0, 0x10);
                found_bus = bus;
                found_slot = slot;
                return bar0 & 0xFFFFFFF0u; /* mask low bits */
            }
        }
    }
    return 0;
}

/* MMIO helper */
static inline void mmio_write32(uint32_t offset, uint32_t value) {
    volatile uint32_t* reg = (volatile uint32_t*)(mmio_base + offset);
    *reg = value;
}

static inline uint32_t mmio_read32(uint32_t offset) {
    volatile uint32_t* reg = (volatile uint32_t*)(mmio_base + offset);
    return *reg;
}

int e1000_init(void) {
    uint32_t bar0 = find_e1000_bar0();
    if (bar0 == 0) {
        print("e1000: device not found\n");
        return -1;
    }

    /* Enable memory and bus-mastering in PCI command register */
    if (found_bus != 0xFF) {
        uint32_t cmd = pci_config_read_dword(found_bus, found_slot, 0, 0x04);
        /* only lower 16 bits are command/status */
        uint16_t cmd16 = cmd & 0xFFFF;
        cmd16 |= 0x6; /* enable memory space and bus master */
        uint32_t newcmd = (cmd & 0xFFFF0000) | cmd16;
        pci_config_write_dword(found_bus, found_slot, 0, 0x04, newcmd);
    }

    /* For simplicity assume BAR0 is memory-mapped at same address for kernel to access
       In real mode we'd need to map physical memory; in QEMU with identity mapping for demo
       we try to cast it. */
    mmio_base = (volatile uint8_t*)(uintptr_t)bar0;

    /* Reset and basic config (very simplified) */
    mmio_write32(REG_CTRL, 0);

    /* Setup Rx descriptors */
    for (int i = 0; i < RX_DESC_COUNT; i++) {
        rx_descs[i].addr = (uint64_t)(uintptr_t)&rx_buffers[i][0];
        rx_descs[i].status = 0;
    }

    mmio_write32(REG_RDBAL, (uint32_t)(uintptr_t)rx_descs);
    mmio_write32(REG_RDBAH, 0);
    mmio_write32(REG_RDLEN, RX_DESC_COUNT * sizeof(struct e1000_rx_desc));
    mmio_write32(REG_RDH, 0);
    mmio_write32(REG_RDT, RX_DESC_COUNT - 1);

    /* Enable receiver — set EN and other sensible flags (simplified) */
    mmio_write32(REG_RCTL, 0x00000002 | (1<<15)); /* EN + long packet */

    /* Setup Tx descriptors (left mostly zeroed) */
    for (int i=0;i<TX_DESC_COUNT;i++) tx_descs[i].status = 1; /* mark as free */
    mmio_write32(REG_TDBAL, (uint32_t)(uintptr_t)tx_descs);
    mmio_write32(REG_TDBAH, 0);
    mmio_write32(REG_TDLEN, TX_DESC_COUNT * sizeof(struct e1000_tx_desc));
    mmio_write32(REG_TDH, 0);
    mmio_write32(REG_TDT, 0);

    /* Enable transmitter */
    mmio_write32(REG_TCTL, 0x00000002);

    /* Read MAC from registers if available */
    uint32_t low = mmio_read32(REG_MAC_LOW);
    uint32_t high = mmio_read32(REG_MAC_HIGH);
    mac_addr[0] = low & 0xFF;
    mac_addr[1] = (low >> 8) & 0xFF;
    mac_addr[2] = (low >> 16) & 0xFF;
    mac_addr[3] = (low >> 24) & 0xFF;
    mac_addr[4] = high & 0xFF;
    mac_addr[5] = (high >> 8) & 0xFF;

    initialized = 1;
    print("e1000: initialized (BAR0=0x");
    /* print hex (very simple) */
    char buf[9];
    uint32_t x = bar0;
    for (int i = 0; i < 8; i++) {
        uint8_t nib = (x >> ((7 - i) * 4)) & 0xF;
        buf[i] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
    }
    buf[8] = '\0';
    print(buf);
    print(")\n");

    print("e1000: MAC=");
    char macbuf[18];
    for (int i = 0; i < 6; i++) {
        uint8_t hi = (mac_addr[i] >> 4) & 0xF;
        uint8_t lo = mac_addr[i] & 0xF;
        macbuf[i*3] = (hi < 10) ? ('0' + hi) : ('A' + hi - 10);
        macbuf[i*3+1] = (lo < 10) ? ('0' + lo) : ('A' + lo - 10);
        macbuf[i*3+2] = (i == 5) ? '\0' : ':';
    }
    print(macbuf);
    print("\n");

    return 0;
}

void e1000_poll(void) {
    if (!initialized) return;
    /* Check Rx descriptors for received packets and print a message if present */
    uint32_t rdh = mmio_read32(REG_RDH);
    uint32_t rdt = mmio_read32(REG_RDT);
    /* Very simplified: scan descriptors for status bit and process */
    for (uint32_t i = 0; i < RX_DESC_COUNT; i++) {
        if (rx_descs[i].status & 0x01) {
            print("e1000: received a packet\n");
            /* call net layer */
            net_receive((const unsigned char*)(uintptr_t)rx_descs[i].addr, rx_descs[i].length);
            /* clear status */
            rx_descs[i].status = 0;
            /* Advance RDT so device can reuse descriptor */
            uint32_t new_rdt = i;
            mmio_write32(REG_RDT, new_rdt);
        }
    }
}

int e1000_send(const uint8_t* buf, int len) {
    if (!initialized) return -1;
    uint32_t idx = tx_tail % TX_DESC_COUNT;
    /* wait until descriptor is free (status DD set by HW when done) */
    if (!(tx_descs[idx].status & 0x1)) {
        /* still in use */
        return -1;
    }
    tx_descs[idx].addr = (uint64_t)(uintptr_t)buf;
    tx_descs[idx].length = (uint16_t)len;
    tx_descs[idx].cmd = 0x1 | 0x8; /* EOP and IFCS */
    tx_descs[idx].status = 0;
    tx_tail++;
    mmio_write32(REG_TDT, tx_tail % TX_DESC_COUNT);
    return 0;
}

void e1000_get_mac(uint8_t mac[6]) {
    for (int i=0;i<6;i++) mac[i] = mac_addr[i];
}
