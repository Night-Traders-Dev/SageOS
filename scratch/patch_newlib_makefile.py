import os

make_path = "/home/kraken/Devel/SageOS/toolchain_build/newlib-4.4.0.20231231/newlib/Makefile.in"

print("Reading Makefile.in...")
with open(make_path, "r") as f:
    content = f.read()

target_append = """@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@am__append_54 = \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/chown.c libc/sys/sageos/close.c libc/sys/sageos/execve.c libc/sys/sageos/fork.c libc/sys/sageos/fstat.c libc/sys/sageos/getenv.c \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/getpid.c libc/sys/sageos/gettod.c libc/sys/sageos/isatty.c libc/sys/sageos/kill.c libc/sys/sageos/link.c libc/sys/sageos/lseek.c libc/sys/sageos/open.c libc/sys/sageos/rdoshelp.c \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/rdos.S libc/sys/sageos/read.c libc/sys/sageos/readlink.c libc/sys/sageos/sbrk.c libc/sys/sageos/stat.c libc/sys/sageos/symlink.c libc/sys/sageos/times.c libc/sys/sageos/unlink.c \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/wait.c libc/sys/sageos/write.c"""

replacement_append = """@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@am__append_54 = \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/close.c libc/sys/sageos/execve.c libc/sys/sageos/fork.c libc/sys/sageos/fstat.c \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/getpid.c libc/sys/sageos/isatty.c libc/sys/sageos/lseek.c libc/sys/sageos/open.c \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/read.c libc/sys/sageos/sbrk.c libc/sys/sageos/times.c libc/sys/sageos/unlink.c \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/wait.c libc/sys/sageos/write.c"""

target_objects = """@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@am__objects_65 = libc/sys/sageos/libc_a-chown.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-close.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-execve.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-fork.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-fstat.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-getenv.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-getpid.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-gettod.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-isatty.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-kill.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-link.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-lseek.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-open.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-rdoshelp.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-rdos.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-read.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-readlink.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-sbrk.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-stat.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-symlink.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-times.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-unlink.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-wait.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-write.$(OBJEXT)"""

replacement_objects = """@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@am__objects_65 = libc/sys/sageos/libc_a-close.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-execve.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-fork.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-fstat.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-getpid.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-isatty.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-lseek.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-open.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-read.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-sbrk.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-times.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-unlink.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-wait.$(OBJEXT) \\
@HAVE_LIBC_SYS_SAGEOS_DIR_TRUE@	libc/sys/sageos/libc_a-write.$(OBJEXT)"""

if target_append in content:
    content = content.replace(target_append, replacement_append)
    print("Successfully replaced am__append_54")
else:
    print("WARNING: target_append not found in Makefile.in!")

if target_objects in content:
    content = content.replace(target_objects, replacement_objects)
    print("Successfully replaced am__objects_65")
else:
    print("WARNING: target_objects not found in Makefile.in!")

with open(make_path, "w") as f:
    f.write(content)
print("Finished patching Makefile.in!")
