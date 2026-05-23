#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Common settings used by most Pico Wi-Fi applications
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1

// Core Module Enabling
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DHCP                   1

// TCP Tuning & Performance Configuration
#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)

// --- Added to fix the sanity check compiler error ---
#define TCP_SND_QUEUELEN            (4 * (TCP_SND_BUF / TCP_MSS))
#define MEMP_NUM_TCP_SEG            TCP_SND_QUEUELEN
// -----------------------------------------------------

// Thread-safety configuration required by the background architecture
#define LINK_SPEED_OF_YOUR_NETIF_IN_BPS 100000000
#define LWIP_CHKSUM_ALGORITHM       3

#endif
