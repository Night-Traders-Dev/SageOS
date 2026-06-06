#ifndef _SYS_UN_H
#define _SYS_UN_H
struct sockaddr_un { int sun_family; char sun_path[108]; };
#endif
