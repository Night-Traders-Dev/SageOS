#ifndef _FCNTL_H
#define _FCNTL_H

#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 00004000

static inline int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; return 0; }

#endif
