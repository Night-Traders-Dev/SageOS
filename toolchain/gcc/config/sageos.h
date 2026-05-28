/* gcc/config/sageos.h — SageOS OS-specific GCC configuration */

#ifndef _SAGEOS_H
#define _SAGEOS_H

/* Identify the OS in preprocessor */
#undef  TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()            \
  do {                                      \
    builtin_define ("__sageos__");          \
    builtin_define ("__unix__");            \
    builtin_assert ("system=sageos");       \
  } while (0)

/* Link against libc; no dynamic linking */
#undef  LIB_SPEC
#define LIB_SPEC "-lc"

/* Startup file */
#undef  STARTFILE_SPEC
#define STARTFILE_SPEC "crt0.o%s"

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC ""

/* No dynamic linker */
#undef  LINK_SPEC
#define LINK_SPEC "-static"

/* No shared libraries */
#undef  SUPPORTS_SHARED
#define SUPPORTS_SHARED 0

/* Size of types — keep consistent with newlib */
#undef  SIZE_TYPE
#define SIZE_TYPE "long unsigned int"

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"

#undef  WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

#endif /* _SAGEOS_H */
