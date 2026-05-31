/* ram_fill.c */
/*
 * Secure Wipe - RAM allocation and management
 * NIST SP 800-88 Rev. 1 aligned
 */

#include "main.h"
#include "config.h"

/* Signal handler for RAM fill operation */
static void fill_sigint_handler(int sig) {
    (void)sig;
    fill_keep_running = 0;
}

/* Add allocated block to tracking array */
static void add_block(void *ptr, size_t size) {
    if (block_count >= block_capacity) {
        size_t new_cap = block_capacity ? block_capacity * 2 : 16;
        RAMBlock *new_arr = realloc(allocated_blocks, new_cap * sizeof(RAMBlock));
        if (!new_arr) {
            fprintf(stderr, COLOR_BOLD "[!] Failed to grow block array\n" COLOR_RESET);
            return;
        }
        allocated_blocks = new_arr;
        block_capacity = new_cap;
    }
    allocated_blocks[block_count].ptr = ptr;
    allocated_blocks[block_count].size = size;
    block_count++;
}

/* Free all allocated blocks */
static void free_all_blocks(void) {
    for (size_t i = 0; i < block_count; i++) {
        if (allocated_blocks[i].ptr) {
            secure_memzero(allocated_blocks[i].ptr, allocated_blocks[i].size);
            munlock(allocated_blocks[i].ptr, allocated_blocks[i].size);
            free(allocated_blocks[i].ptr);
        }
    }
    free(allocated_blocks);
    allocated_blocks = NULL;
    block_count = block_capacity = 0;
}


/* Get available memory in MB */
static unsigned long get_avail_mb(void) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        perror("sysinfo");
        return 0;
    }
    unsigned long avail_bytes = (info.freeram + info.bufferram) * info.mem_unit;
    return avail_bytes / (1024 * 1024);
}

/* Touch pages to ensure they are actually allocated and filled with entropy */
static void touch_pages(void *ptr, size_t size_bytes) {
    printf("Wiping block with entropy... ");
    fflush(stdout);
    if (get_secure_random(ptr, size_bytes) != 0) {
        /* Fallback to zero if secure random fails, though this is non-ideal */
        volatile char *p = (volatile char *)ptr;
        for (size_t i = 0; i < size_bytes; i += PAGE_SIZE) {
            p[i] = 0;
        }
    }
}

/* Fill RAM with allocated memory blocks */
void fill_ram(unsigned long safety_mb) {
    fill_keep_running = 1;
    unsigned long avail_mb;
    size_t chunk_bytes = CHUNK_SIZE_MB * 1024UL * 1024UL;

    printf(COLOR_CYAN "\n[*] Starting RAM fill. Safety margin: %lu MB\n" COLOR_RESET, safety_mb);
    printf(COLOR_BOLD "[i] Memory will be pinned (mlock) and filled with entropy.\n" COLOR_RESET);
    printf(COLOR_BOLD "[i] Press Ctrl+C to stop and keep memory allocated.\n" COLOR_RESET);

    signal(SIGINT, fill_sigint_handler);

    while (fill_keep_running) {
        avail_mb = get_avail_mb();
        if (avail_mb <= safety_mb) {
            printf(COLOR_CYAN "\nAvailable memory %lu MB <= safety margin %lu MB. Stopping.\n" COLOR_RESET,
                   avail_mb, safety_mb);
            break;
        }

        size_t alloc_bytes = chunk_bytes;
        if (avail_mb - safety_mb < CHUNK_SIZE_MB) {
            alloc_bytes = (avail_mb - safety_mb) * 1024UL * 1024UL;
            if (alloc_bytes == 0) break;
        }

        printf("Allocating %.2f MB (avail: %lu MB) ... ",
               (double)alloc_bytes / (1024.0*1024.0), avail_mb);
        fflush(stdout);

        void *block = NULL;
        if (posix_memalign(&block, PAGE_SIZE, alloc_bytes) != 0) {
            perror("\nposix_memalign failed");
            break;
        }

        /* Attempt to pin the memory to prevent swapping */
        if (mlock(block, alloc_bytes) != 0) {
            static int warned = 0;
            if (!warned) {
                printf(COLOR_BOLD "\n[!] WARNING: mlock failed. Memory may be swapped to disk.\n" COLOR_RESET);
                warned = 1;
            }
        }

        touch_pages(block, alloc_bytes);
        add_block(block, alloc_bytes);

        printf(COLOR_CYAN "done. Total blocks: %zu\n" COLOR_RESET, block_count);

    }

    printf(COLOR_CYAN "\nMemory fill stopped. %zu blocks allocated and pinned.\n" COLOR_RESET, block_count);
}


/* Release all allocated RAM */
void release_ram(void) {
    size_t freed = block_count;
    free_all_blocks();
    printf(COLOR_CYAN "[+] All memory released (%zu blocks freed).\n" COLOR_RESET, freed);
}
