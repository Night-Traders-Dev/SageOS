#ifndef _NETINET_IN_H
#define _NETINET_IN_H
#include <stdint.h>
typedef uint32_t in_addr_t;
struct in_addr { in_addr_t s_addr; };
struct sockaddr_in { int sin_family; uint16_t sin_port; struct in_addr sin_addr; char sin_zero[8]; };
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
static inline uint16_t htons(uint16_t x) { return (x << 8) | (x >> 8); }
static inline uint16_t ntohs(uint16_t x) { return (x << 8) | (x >> 8); }
#endif
