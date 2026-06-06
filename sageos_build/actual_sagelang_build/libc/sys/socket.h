#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H
#include <stddef.h>
typedef int socklen_t;
typedef int sa_family_t;
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
#define AF_INET 2
#define AF_INET6 10
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
static inline int socket(int domain, int type, int protocol) { (void)domain;(void)type;(void)protocol; return -1; }
static inline int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) { (void)sockfd;(void)addr;(void)addrlen; return -1; }
static inline int listen(int sockfd, int backlog) { (void)sockfd;(void)backlog; return -1; }
static inline int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) { (void)sockfd;(void)addr;(void)addrlen; return -1; }
static inline int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) { (void)sockfd;(void)addr;(void)addrlen; return -1; }
static inline int send(int sockfd, const void *buf, size_t len, int flags) { (void)sockfd;(void)buf;(void)len;(void)flags; return -1; }
static inline int recv(int sockfd, void *buf, size_t len, int flags) { (void)sockfd;(void)buf;(void)len;(void)flags; return -1; }
static inline int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) { (void)sockfd;(void)level;(void)optname;(void)optval;(void)optlen; return -1; }
#endif
