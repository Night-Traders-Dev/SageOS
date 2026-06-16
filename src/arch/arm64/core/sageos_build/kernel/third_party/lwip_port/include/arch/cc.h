#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>

extern void console_write(const char *s);
extern int sage_printf(const char *fmt, ...);
extern void dmesg_printf(const char *fmt, ...);

#define LWIP_PLATFORM_DIAG(x) do { dmesg_printf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { console_write("LWIP ASSERT: "); console_write(x); console_write("\n"); } while(0)

#define X8_F  "02x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

// We don't have rand() in freestanding
extern uint64_t timer_ticks(void);
#define LWIP_RAND() ((uint32_t)timer_ticks())

typedef uint32_t sys_prot_t;

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#endif
