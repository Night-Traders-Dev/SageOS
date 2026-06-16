# toolchain/newlib/libc/sys/sageos/Makefile.inc
noinst_LIBRARIES = lib.a
lib_a_SOURCES = crt0.S syscalls.c
lib_a_CFLAGS = $(AM_CFLAGS)
