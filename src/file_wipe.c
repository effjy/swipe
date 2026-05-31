/* file_wipe.c */
/*
 * Secure Wipe - File wiping functionality
 * NIST SP 800-88 Rev. 1 aligned
 */

#include "main.h"
#include "config.h"

/* Securely wipe and delete a file */
int wipe_file(const char *path) {
    g_stop_flag = 0;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, COLOR_BOLD "[!] Not a regular file or inaccessible: %s\n" COLOR_RESET, path);
        return -1;
    }

    if (st.st_size <= 0) {
        printf(COLOR_BOLD "[i] Skipping empty file: %s\n" COLOR_RESET, path);
        return 0;
    }

    size_t file_size = (size_t)st.st_size;
    g_target_bytes = file_size * schemes[current_scheme_idx].pass_count;
    g_bytes_written = 0;
    g_start_time = time(NULL);
    printf("\n" COLOR_CYAN "[*] Sanitizing: %s (%.2f MB)\n" COLOR_RESET, path, file_size / 1048576.0);

    /* Attempt to use O_DIRECT to bypass page cache */
    int flags = O_RDWR | O_CLOEXEC;
#ifdef O_DIRECT
    flags |= O_DIRECT;
#endif
    int fd = open(path, flags);
    if (fd < 0 && (errno == EINVAL)) {
        /* Fallback if O_DIRECT is not supported by filesystem */
        fd = open(path, O_RDWR | O_CLOEXEC);
    }
    if (fd < 0) { perror(COLOR_BOLD "[!] open" COLOR_RESET); return -1; }

    /* Advise the kernel that we don't want this data in cache */
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);

    unsigned char *buf = aligned_alloc(4096, BUFFER_SIZE);
    if (!buf) {
        perror(COLOR_BOLD "[!] aligned_alloc" COLOR_RESET);
        close(fd);
        return -1;
    }
    int res = 0;
    if (g_mlock_supported && mlock(buf, BUFFER_SIZE) != 0) g_mlock_supported = 0;
    size_t offset = 0;
    size_t last_update_bytes = 0;

    for (int p = 0; p < schemes[current_scheme_idx].pass_count && !g_stop_flag; p++) {
        PassType type = schemes[current_scheme_idx].passes[p];
        if (fill_buffer(buf, BUFFER_SIZE, type) != 0) { res = -1; break; }
        offset = 0;
        while (offset < file_size) {
            size_t to_write = (file_size - offset > BUFFER_SIZE) ? BUFFER_SIZE : (file_size - offset);
            
            /* Ensure write size is block-aligned for O_DIRECT if needed */
            /* If it's the last chunk, we might need to handle it specially if O_DIRECT is on */
            /* But BUFFER_SIZE and offset should be fine if file_size is large. */
            /* For the very last chunk that isn't aligned, we might get an error with O_DIRECT. */
            
            lseek(fd, (off_t)offset, SEEK_SET);
            if (type == PASS_VERIFY) {
                if (read(fd, buf, to_write) != (ssize_t)to_write) {
                    /* Fallback for O_DIRECT alignment issues */
                    if (errno == EINVAL) {
                        int old_flags = fcntl(fd, F_GETFL);
                        fcntl(fd, F_SETFL, old_flags & ~O_DIRECT);
                        if (read(fd, buf, to_write) != (ssize_t)to_write) { res = -1; break; }
                        fcntl(fd, F_SETFL, old_flags);
                    } else {
                        res = -1; break;
                    }
                }
            } else {

                if (write(fd, buf, to_write) != (ssize_t)to_write) {
                    /* If O_DIRECT failed due to alignment, try one more time without it for this chunk */
                    if (errno == EINVAL) {
                        int old_flags = fcntl(fd, F_GETFL);
                        fcntl(fd, F_SETFL, old_flags & ~O_DIRECT);
                        if (write(fd, buf, to_write) != (ssize_t)to_write) { res = -1; break; }
                        fcntl(fd, F_SETFL, old_flags);
                    } else {
                        res = -1; break;
                    }
                }
            }
            g_bytes_written += to_write;
            offset += to_write;
            if (g_bytes_written - last_update_bytes >= PROGRESS_UPDATE_INTERVAL) {
                update_progress("Sanitizing...");
                last_update_bytes = g_bytes_written;
                if (check_for_stop_interrupt()) { res = -2; break; }
            }
        }
        if (res != 0) break;
        fsync(fd);
        posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
        update_progress("Sanitizing...");
    }
    secure_memzero(buf, BUFFER_SIZE);
    if (g_mlock_supported) munlock(buf, BUFFER_SIZE);
    free(buf);
    
    close(fd);

    if (res == 0) {
        char *dir = strdup(path);
        if (dir) {
            char *last_slash = strrchr(dir, '/');
            if (last_slash) *last_slash = '\0'; else strcpy(dir, ".");
            
            char current_path[4096];
            snprintf(current_path, sizeof(current_path), "%s", path);

            /* Multiple renames for metadata obfuscation */
            for (int i = 0; i < 7; i++) {
                char new_name[4096];
                unsigned char rb[12];
                if (get_secure_random(rb, 12) != 0) {
                    snprintf(new_name, sizeof(new_name), "%s/.secure_tmp_%lu_%d.tmp", dir, (unsigned long)time(NULL), i);
                } else {
                    char hex[25];
                    for(int j=0; j<12; j++) sprintf(hex+(j*2), "%02x", rb[j]);
                    snprintf(new_name, sizeof(new_name), "%s/.%s.tmp", dir, hex);
                }
                if (rename(current_path, new_name) == 0) {
                    snprintf(current_path, sizeof(current_path), "%s", new_name);
                } else {
                    break;
                }
            }

            int dfd = open(dir, O_RDONLY);
            if (dfd >= 0) { fsync(dfd); close(dfd); }
            if (unlink(current_path) == 0) attempt_trim(path);
            free(dir);
        }
        printf("\n" COLOR_CYAN "[+] File securely sanitized & removed: %s\n" COLOR_RESET, path);
    } else if (res == -2) {
        printf("\n" COLOR_BOLD "[!] Sanitization interrupted. File remains on disk.\n" COLOR_RESET);
    }
    return res;
}

