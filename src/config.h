/* config.h */
/*
 * Secure Wipe - Configuration and sanitization schemes
 * NIST SP 800-88 Rev. 1 aligned
 */

#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "2.0.0"

#include <stddef.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <termios.h>
#include <stdatomic.h>
#include <pthread.h>

/* Configuration constants */
#define SAFE_ZONE_BYTES 0ULL
#define BUFFER_SIZE (1ULL * 1024 * 1024)
#define PROGRESS_UPDATE_INTERVAL (4ULL * 1024 * 1024)

/* RAM fill parameters */
#define DEFAULT_SAFETY_MB 250
#define CHUNK_SIZE_MB     64UL
#define PAGE_SIZE         4096

/* Pass types */
typedef enum {
    PASS_ZERO = 0,
    PASS_ONES,
    PASS_RANDOM,
    PASS_VERIFY
} PassType;

/* Wipe scheme structure */
typedef struct {
    int id;
    const char *name;
    const char *standard;
    PassType passes[10];
    int pass_count;
} WipeScheme;

/* NIST SP 800-88 Rev. 1 Aligned Sanitization Schemes */
extern const WipeScheme schemes[];
extern int current_scheme_idx;

/* Global state variables */
extern volatile sig_atomic_t g_stop_flag;
extern _Atomic size_t g_bytes_written;
extern volatile size_t g_target_bytes;
extern volatile time_t g_start_time;
extern struct termios g_original_termios;
extern int g_termios_saved;
extern int g_mlock_supported;

/* RAM block tracking structure */
typedef struct {
    void *ptr;
    size_t size;
} RAMBlock;

/* RAM fill state */
extern RAMBlock *allocated_blocks;
extern size_t block_count;
extern size_t block_capacity;
extern volatile sig_atomic_t fill_keep_running;

#endif /* CONFIG_H */
