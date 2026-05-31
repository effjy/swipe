/* main.h */
/*
 * Secure Wipe - Classified data sanitization tool
 * NIST SP 800-88 Rev. 1 aligned
 */

#ifndef MAIN_H
#define MAIN_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/random.h>
#include <sys/vfs.h>
#ifdef __linux__
#include <linux/fs.h>
#endif
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <poll.h>
#include <termios.h>
#include <signal.h>
#include <stdint.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>

#ifndef O_DIRECT
#define O_DIRECT 00040000
#endif


/* ANSI Color Codes */
#define COLOR_WHITE   "\x1b[37m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_BOLD    "\x1b[1m"
#define COLOR_RESET   "\x1b[0m"

/* Function prototypes */
void secure_memzero(void *ptr, size_t len);
int get_secure_random(void *buf, size_t len);
int fill_buffer(unsigned char *buf, size_t len, int type);
void attempt_trim(const char *path);
void restore_terminal(void);
int check_for_stop_interrupt(void);
void update_progress(const char *label);
void startup_compliance_check(void);
void fill_ram(unsigned long safety_mb);
void release_ram(void);
int wipe_file(const char *path);
int wipe_directory_recursive(const char *path);
int wipe_free_space(const char *path);
void show_settings(void);

#endif /* MAIN_H */
