/* directory_wipe.c */
/*
 * Secure Wipe - Directory and free space wiping
 * NIST SP 800-88 Rev. 1 aligned
 */

#include "main.h"
#include "config.h"

/* Recursively wipe a directory */
int wipe_directory_recursive(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) { perror(COLOR_BOLD "[!] opendir" COLOR_RESET); return -1; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        struct stat st;
        if (lstat(fullpath, &st) != 0) continue;

        if (S_ISLNK(st.st_mode)) {
            /* If it's a symlink, just remove the link itself. 
               Wiping the target would be dangerous as it might be outside the target dir. */
            unlink(fullpath);
        } else if (S_ISDIR(st.st_mode)) {
            wipe_directory_recursive(fullpath);
            rmdir(fullpath);
        } else if (S_ISREG(st.st_mode)) {
            wipe_file(fullpath);
        } else {
            /* Handle other special files (sockets, pipes, etc) by just unlinking */
            unlink(fullpath);
        }
    }
    closedir(dir);
    return 0;
}


#define WIPE_THREADS 8
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char path[4096];
    size_t bytes_to_wipe;
    int thread_id;
} WipeWorkerArgs;

static void *free_space_worker(void *arg) {
    WipeWorkerArgs *wa = (WipeWorkerArgs *)arg;
    unsigned char *buf = aligned_alloc(4096, BUFFER_SIZE);
    if (!buf) return NULL;
    if (g_mlock_supported && mlock(buf, BUFFER_SIZE) != 0) g_mlock_supported = 0;

    size_t written_by_this_thread = 0;
    while (written_by_this_thread < wa->bytes_to_wipe && !g_stop_flag) {
        size_t remaining = wa->bytes_to_wipe - written_by_this_thread;
        size_t chunk = (remaining > BUFFER_SIZE * 20) ? BUFFER_SIZE * 20 : remaining;

        char tmp_template[4224];
        snprintf(tmp_template, sizeof(tmp_template), "%s/.secure_wipe_t%d_c%zu_XXXXXX", wa->path, wa->thread_id, written_by_this_thread);
        char *tmpname = strdup(tmp_template);
        int fd = mkstemp(tmpname);
        if (fd < 0) {
            free(tmpname);
            if (errno == ENOSPC) {
                break; // Disk is full, this thread is done
            }
            g_stop_flag = 1;
            break;
        }

#ifdef O_DIRECT
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_DIRECT);
#endif
        posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);

        int write_failed = 0;
        for (int p = 0; p < schemes[current_scheme_idx].pass_count && !g_stop_flag; p++) {
            if (fill_buffer(buf, BUFFER_SIZE, schemes[current_scheme_idx].passes[p]) != 0) {
                g_stop_flag = 1; write_failed = 1; break;
            }
            lseek(fd, 0, SEEK_SET);
            size_t off = 0;
            while (off < chunk && !g_stop_flag) {
                size_t tw = (chunk - off > BUFFER_SIZE) ? BUFFER_SIZE : (chunk - off);
                if (write(fd, buf, tw) != (ssize_t)tw) {
                    if (errno == EINVAL) {
                        int old_flags = fcntl(fd, F_GETFL);
                        fcntl(fd, F_SETFL, old_flags & ~O_DIRECT);
                        if (write(fd, buf, tw) != (ssize_t)tw) {
                            if (errno == ENOSPC) { write_failed = 2; } else { g_stop_flag = 1; write_failed = 1; }
                            break;
                        }
                        fcntl(fd, F_SETFL, old_flags);
                    } else {
                        if (errno == ENOSPC) { write_failed = 2; } else { g_stop_flag = 1; write_failed = 1; }
                        break;
                    }
                }
                off += tw;
                atomic_fetch_add(&g_bytes_written, tw);
            }
            if (write_failed) break;
            fsync(fd);
            posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
        }

        close(fd);
        free(tmpname);

        if (write_failed) {
            if (write_failed == 2) {
                break; // ENOSPC: disk is full, stop this thread normally
            }
            break; // Other failure, stop
        }

        written_by_this_thread += chunk;
    }

    secure_memzero(buf, BUFFER_SIZE);
    if (g_mlock_supported) munlock(buf, BUFFER_SIZE);
    free(buf);
    return NULL;
}

/* Wipe free space on a filesystem - Multi-threaded version */
int wipe_free_space(const char *path) {
    g_stop_flag = 0;
    struct statvfs st;
    if (statvfs(path, &st) != 0) { perror(COLOR_BOLD "[!] statvfs" COLOR_RESET); return -1; }
    uint64_t avail = (uint64_t)st.f_bavail * (uint64_t)st.f_bsize;
    if (avail <= SAFE_ZONE_BYTES && SAFE_ZONE_BYTES > 0) {
        printf(COLOR_BOLD "[!] Less than %llu bytes available. Aborting.\n" COLOR_RESET, SAFE_ZONE_BYTES);
        return -1;
    }

    size_t total_disk_to_wipe = (size_t)(avail - SAFE_ZONE_BYTES);
    g_target_bytes = total_disk_to_wipe * schemes[current_scheme_idx].pass_count;
    g_bytes_written = 0; 
    g_start_time = time(NULL);

    printf("\n" COLOR_CYAN "[*] Sanitizing free space on: %s\n" COLOR_RESET, path);
    printf(COLOR_CYAN "[*] Target disk: %.2f GB (Parallel Engine: %d threads)\n" COLOR_RESET, 
           (double)total_disk_to_wipe / 1073741824.0, WIPE_THREADS);
    printf(COLOR_BOLD "[i] Press 's' then Enter to stop & cleanup.\n" COLOR_RESET);

    tcgetattr(STDIN_FILENO, &g_original_termios);
    struct termios newt = g_original_termios;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    g_termios_saved = 1; atexit(restore_terminal);

    pthread_t threads[WIPE_THREADS];
    WipeWorkerArgs args[WIPE_THREADS];
    size_t per_thread = total_disk_to_wipe / WIPE_THREADS;

    for (int i = 0; i < WIPE_THREADS; i++) {
        snprintf(args[i].path, sizeof(args[i].path), "%s", path);
        args[i].thread_id = i;
        args[i].bytes_to_wipe = (i == WIPE_THREADS - 1) ? (total_disk_to_wipe - (per_thread * i)) : per_thread;
        pthread_create(&threads[i], NULL, free_space_worker, &args[i]);
    }

    int joined[WIPE_THREADS] = {0};
    size_t last_update_bytes = 0;
    while (!g_stop_flag) {
        int active = 0;
        for (int i = 0; i < WIPE_THREADS; i++) {
            if (!joined[i]) {
                if (pthread_tryjoin_np(threads[i], NULL) == 0) {
                    joined[i] = 1;
                } else {
                    active = 1;
                }
            }
        }
        if (!active) break;

        size_t current_written = atomic_load(&g_bytes_written);
        if (current_written - last_update_bytes >= PROGRESS_UPDATE_INTERVAL) {
            pthread_mutex_lock(&log_mutex);
            update_progress("Sanitizing Free Space...");
            pthread_mutex_unlock(&log_mutex);
            last_update_bytes = current_written;
        }
        if (check_for_stop_interrupt()) g_stop_flag = 1;
        usleep(100000);
    }

    for (int i = 0; i < WIPE_THREADS; i++) {
        if (!joined[i]) {
            pthread_join(threads[i], NULL);
        }
    }

    restore_terminal();
    printf("\n" COLOR_CYAN "[*] Finalizing and flushing to disk...\n" COLOR_RESET);
    attempt_trim(path); sync();

    // Clean up all temporary files matching ".secure_wipe_t" in the directory
    DIR *dir = opendir(path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, ".secure_wipe_t", 14) == 0) {
                char fullpath[4096];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
                unlink(fullpath);
            }
        }
        closedir(dir);
    }

    printf(COLOR_CYAN "[+] Free space sanitization & cleanup complete.\n" COLOR_RESET);
    return 0;
}
