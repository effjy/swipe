/* compliance.c */
/*
 * Secure Wipe - Startup compliance checks
 * NIST SP 800-88 Rev. 1 aligned
 */

#include "main.h"
#include "config.h"

/* Perform startup compliance checks */
void startup_compliance_check(void) {
    int warnings = 0;

    void *test_buf = aligned_alloc(4096, BUFFER_SIZE);
    if (test_buf) {
        if (mlock(test_buf, BUFFER_SIZE) == 0) {
            g_mlock_supported = 1;
            munlock(test_buf, BUFFER_SIZE);
        } else {
            printf(COLOR_BOLD "[!] WARNING: mlock unavailable. Memory may page to disk.\n" COLOR_RESET);
            warnings++;
        }
        free(test_buf);
    }

    struct statfs stfs;
    if (statfs(".", &stfs) == 0) {
        uint64_t fs_types[] = {0xEF53ULL, 0x58465342ULL, 0x9123683EULL, 0x2FC12FC1ULL, 0xF2F52010ULL, 0};
        for (int i = 0; fs_types[i] != 0; i++) {
            if ((uint64_t)stfs.f_type == fs_types[i]) {
                printf(COLOR_BOLD "[!] WARNING: Journal/CoW filesystem detected. Metadata/journal may retain residual data.\n" COLOR_RESET);
                warnings++;
                break;
            }
        }
    }

    /* SSD Detection */
    struct stat st;
    if (stat(".", &st) == 0) {
        char sys_path[256];
        snprintf(sys_path, sizeof(sys_path), "/sys/dev/block/%d:%d/queue/rotational", major(st.st_dev), minor(st.st_dev));
        FILE *f = fopen(sys_path, "r");
        if (f) {
            int rotational;
            if (fscanf(f, "%d", &rotational) == 1 && rotational == 0) {
                printf(COLOR_BOLD "[!] WARNING: SSD/NVMe detected. Software-level wiping is less effective due to wear-leveling.\n" COLOR_RESET);
                printf(COLOR_BOLD "[i] Recommendation: Use the drive's internal 'Secure Erase' command if available.\n" COLOR_RESET);
                warnings++;
            }
            fclose(f);
        }
    }

    if (warnings == 0) {
        printf(COLOR_CYAN "[i] Compliance checks passed. Operating in classified mode.\n" COLOR_RESET);
    } else {
        printf(COLOR_BOLD "[i] Fallbacks enabled. Proceeding with best-effort sanitization.\n" COLOR_RESET);
    }
}

