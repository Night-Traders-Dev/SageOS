#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>

int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);

// fork is handled by macro
int execv(const char *path, char *const argv[]);

unsigned int sleep(unsigned int seconds);

#define _SC_NPROCESSORS_ONLN 1
static inline long sysconf(int name) { (void)name; return 1; }

#endif
