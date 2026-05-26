#ifndef _DIRENT_H
#define _DIRENT_H

struct dirent {
    char d_name[256];
};

typedef void DIR;

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#endif
