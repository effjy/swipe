/* terminal_utils.c */
/*
 * Secure Wipe - Terminal handling and progress display
 * NIST SP 800-88 Rev. 1 aligned
 */

#include "main.h"
#include "config.h"

/* Restore original terminal settings */
void restore_terminal(void) {
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
        g_termios_saved = 0;
    }
}

/* Check for stop interrupt from user */
int check_for_stop_interrupt(void) {
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    if (poll(&pfd, 1, 50) > 0) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 's' || c == 'S') {
                g_stop_flag = 1;
                return 1;
            }
        }
    }
    return 0;
}

/* Update progress display */
void update_progress(const char *label) {
    if (g_target_bytes == 0) return;
    double pct = (double)g_bytes_written / (double)g_target_bytes * 100.0;
    double elapsed = difftime(time(NULL), g_start_time);
    double speed = (elapsed > 0) ? (g_bytes_written / (1024.0 * 1024.0)) / elapsed : 0.0;
    long long remaining_sec = (speed > 0) ? (long long)((g_target_bytes - g_bytes_written) / (speed * 1024.0 * 1024.0)) : 0;
    int mins = (int)(remaining_sec / 60);
    int secs = (int)(remaining_sec % 60);
    int bar_len = 30;
    int filled = (int)(pct / 100.0 * bar_len);
    
    if (filled < 0) filled = 0;
    if (filled > bar_len) filled = bar_len;
    
    char bar[32] = {0};
    for (int i = 0; i < bar_len; i++) bar[i] = (i < filled) ? '#' : ' ';
    bar[bar_len] = '\0';
    printf("\r[" COLOR_CYAN "%s" COLOR_RESET "] %5.1f%% | %6.2f MB/s | ETA: %02d:%02d | %s",
           bar, pct, speed, mins, secs, label);
    fflush(stdout);
}
