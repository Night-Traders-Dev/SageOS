#include <mbedtls/platform.h>
#include <mbedtls/entropy.h>
#include <mbedtls/debug.h>
#include <psa/crypto.h>
#include "sage_libc_shim.h"
#include "dmesg.h"
#include "timer.h"
#include "lwip/mem.h"

/* mbed TLS platform functions */

static void *mbedtls_port_calloc(size_t n, size_t size) {
    void *ptr = mem_malloc((mem_size_t)(n * size));
    if (ptr) {
        sage_memset(ptr, 0, n * size);
    }
    return ptr;
}

static void mbedtls_port_free(void *ptr) {
    if (ptr) {
        mem_free(ptr);
    }
}

void mbedtls_platform_exit_alt(int status) {
    (void)status;
    // Just loop or do nothing in kernel
    for(;;);
}

static int supports_rdrand(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1));
    return (ecx & (1 << 30)) != 0;
}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    (void)data;
    size_t i = 0;

    if (supports_rdrand()) {
        while (i < len) {
            uint64_t val = 0;
            unsigned char ok = 0;
            __asm__ volatile("rdrand %0; setc %1"
                             : "=r"(val), "=qm"(ok)
                             :
                             : "cc");
            if (ok) {
                size_t to_copy = (len - i < 8) ? (len - i) : 8;
                sage_memcpy(output + i, &val, to_copy);
                i += to_copy;
            } else {
                break;
            }
        }
    }

    uint64_t seed = timer_ticks();
    while (i < len) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        uint64_t t = timer_ticks() ^ seed;
        size_t to_copy = (len - i < 8) ? (len - i) : 8;
        sage_memcpy(output + i, &t, to_copy);
        i += to_copy;
    }

    *olen = len;
    return 0;
}


void mbedtls_port_init(void) {
    /* Configure MbedTLS to use persistent lwIP memory functions */
    mbedtls_platform_set_calloc_free(mbedtls_port_calloc, mbedtls_port_free);
    
    /* Set debug threshold to 1 for standard logs */
    mbedtls_debug_set_threshold(1);
    
    /* Initialize PSA Crypto - REQUIRED for TLS 1.3 in MbedTLS 3.x */
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        dmesg_printf("PSA: initialization failed (status=%d)", (int)status);
    } else {
        dmesg_log("PSA: initialization successful");
    }
}
