/* secure_utils.c */
/*
 * Secure Wipe - Secure memory and random utilities
 * NIST SP 800-88 Rev. 1 aligned
 */

#include "main.h"
#include "config.h"

/* Secure memory zeroing that won't be optimized away */
void secure_memzero(void *ptr, size_t len) {
    volatile unsigned char *p = ptr;
    while (len--) *p++ = 0;
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
}

/* Get cryptographically secure random bytes */
int get_secure_random(void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t r = getrandom((char *)buf + total, len - total, 0);
        if (r <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)r;
    }
    return 0;
}


/* Fill buffer with pattern based on pass type */
int fill_buffer(unsigned char *buf, size_t len, int type) {
    switch (type) {
        case PASS_ZERO:  memset(buf, 0x00, len); break;
        case PASS_ONES:  memset(buf, 0xFF, len); break;
        case PASS_RANDOM:
            if (get_secure_random(buf, len) != 0) {
                fprintf(stderr, "\n" COLOR_BOLD "[!] Secure random generation failed. Aborting.\n" COLOR_RESET);
                return -1;
            }
            break;
        case PASS_VERIFY: break;
    }
    return 0;
}
