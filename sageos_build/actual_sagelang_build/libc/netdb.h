#ifndef _NETDB_H
#define _NETDB_H
#include <sys/socket.h>
struct hostent { char* h_name; char** h_aliases; int h_addrtype; int h_length; char** h_addr_list; };
#define h_addr h_addr_list[0]
static inline struct hostent* gethostbyname(const char* name) { (void)name; return 0; }

struct addrinfo { int ai_flags; int ai_family; int ai_socktype; int ai_protocol; socklen_t ai_addrlen; struct sockaddr *ai_addr; char *ai_canonname; struct addrinfo *ai_next; };
static inline int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) { (void)node;(void)service;(void)hints;(void)res; return -1; }
static inline void freeaddrinfo(struct addrinfo *res) { (void)res; }
#endif
