// lwIP (TCP/IP stack) settings for Wi-Fi OTA.
// Based on pico-examples pico_w/wifi/lwipopts_examples_common.h.
// Options: https://www.nongnu.org/lwip/2_1_x/group__lwip__opts.html
#pragma once

#define NO_SYS 1 // no RTOS: lwIP runs in the cyw43 background context
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#define MEM_LIBC_MALLOC 0 // not allowed with threadsafe_background
#define MEM_ALIGNMENT 4
#define MEM_SIZE 4000
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_ARP_QUEUE 10
#define PBUF_POOL_SIZE 24
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1 // answers ping
#define LWIP_RAW 1
#define TCP_MSS 1460
#define TCP_WND (8 * TCP_MSS)
#define TCP_SND_BUF (8 * TCP_MSS)
#define TCP_SND_QUEUELEN ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1 // DHCP sends CYW43_HOST_NAME to the router
#define MEM_STATS 0
#define SYS_STATS 0
#define MEMP_STATS 0
#define LINK_STATS 0
#define LWIP_CHKSUM_ALGORITHM 3
#define LWIP_DHCP 1
#define LWIP_IPV4 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_DNS 0
#define LWIP_TCP_KEEPALIVE 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1
#define DHCP_DOES_ARP_CHECK 0
#define LWIP_DHCP_DOES_ACD_CHECK 0

// mDNS responder: the PC finds the car as <CYW43_HOST_NAME>.local
#define LWIP_MDNS_RESPONDER 1
#define LWIP_IGMP 1                        // mDNS uses multicast
#define LWIP_NUM_NETIF_CLIENT_DATA 1       // mDNS keeps its state on the netif
#define LWIP_NETIF_EXT_STATUS_CALLBACK 1   // mDNS re-announces on IP change
#define MDNS_MAX_SERVICES 1
#define MEMP_NUM_SYS_TIMEOUT (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 8)
