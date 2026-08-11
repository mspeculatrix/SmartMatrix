#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// --- Core Settings ---
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1

// --- Core Protocols ---
#define LWIP_IPV4                   1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DHCP                   1

// --- Memory & Heap Sizing (CRITICAL FOR PICO 2 W) ---
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    16000      // Allocates heap for connection control blocks
#define PBUF_POOL_SIZE              24         // Network buffer pool size
#define MEMP_NUM_ARP_QUEUE          10

// --- TCP Tuning & Buffers ---
#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define MEMP_NUM_TCP_SEG            32

// --- Fast DHCP & Callbacks ---
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1

// --- Architecture & Hardware Tuning ---
#define LINK_SPEED_OF_YOUR_NETIF_IN_BPS 100000000
#define LWIP_CHKSUM_ALGORITHM       3

#endif /* _LWIPOPTS_H */
