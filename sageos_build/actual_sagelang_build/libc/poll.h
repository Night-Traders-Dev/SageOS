#ifndef _POLL_H
#define _POLL_H
struct pollfd { int fd; short events; short revents; };
#define POLLIN 1
static inline int poll(struct pollfd *fds, int nfds, int timeout) { (void)fds;(void)nfds;(void)timeout; return 0; }
#endif
