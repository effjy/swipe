/* trim_utils.c */
/*
 * Secure Wipe - TRIM and cache purge utilities
 * NIST SP 800-88 Rev. 1 aligned
 */

#include "main.h"
#include "config.h"

/* Attempt to TRIM filesystem to ensure data removal */
void attempt_trim(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) { sync(); return; }

    int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) { sync(); return; }

#if defined(__linux__) && defined(FITRIM)
    struct fstrim_range range = { .start = 0, .len = ~0ULL, .minlen = 4096 };
    ioctl(fd, FITRIM, &range);
#endif
    sync();
    close(fd);
}
