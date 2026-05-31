/* config.c */
/*
 * Secure Wipe - Configuration and sanitization schemes implementation
 * NIST SP 800-88 Rev. 1 aligned
 */

#include "config.h"

/* Global state variables */
int current_scheme_idx = 3;
volatile sig_atomic_t g_stop_flag = 0;
_Atomic size_t g_bytes_written = 0;
volatile size_t g_target_bytes = 0;
volatile time_t g_start_time = 0;
struct termios g_original_termios;
int g_termios_saved = 0;
int g_mlock_supported = 0;

/* RAM fill state */
RAMBlock *allocated_blocks = NULL;
size_t block_count = 0;
size_t block_capacity = 0;
volatile sig_atomic_t fill_keep_running = 1;


/* NIST SP 800-88 Rev. 1 Aligned Sanitization Schemes */
const WipeScheme schemes[] = {
    {1, "NIST Clear (Baseline)",        "NIST SP 800-88 Rev. 1 §4.1",  {PASS_ZERO}, 1},
    {2, "DoD 5220.22-M (Overwrite)",    "DoD 5220.22-M (E)",          {PASS_ZERO, PASS_ONES, PASS_RANDOM}, 3},
    {3, "NIST Purge (Multi-Pass)",      "NIST SP 800-88 Rev. 1 §4.2",  {PASS_ZERO, PASS_ONES, PASS_RANDOM, PASS_VERIFY}, 4},
    {4, "FIPS High-Entropy Purge",      "FIPS 140-3 / NIST 800-88",    {PASS_RANDOM, PASS_RANDOM, PASS_ZERO, PASS_RANDOM, PASS_VERIFY}, 5}
};
