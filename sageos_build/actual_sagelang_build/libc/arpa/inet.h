#ifndef _ARPA_INET_H
#define _ARPA_INET_H
#include <netinet/in.h>
static inline char* inet_ntoa(struct in_addr in) { (void)in; return "0.0.0.0"; }
static inline int inet_aton(const char* cp, struct in_addr* inp) { (void)cp;(void)inp; return 0; }
static inline int inet_pton(int af, const char* src, void* dst) { (void)af;(void)src;(void)dst; return 0; }
#endif
