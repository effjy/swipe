/* main.c */
/*
 * Secure Wipe - Main function and menu system
 * NIST SP 800-88 Rev. 1 aligned
 */

#include "main.h"
#include "config.h"

/* Show sanitization settings and allow changes */
void show_settings(void) {
    printf("\n" COLOR_BOLD COLOR_CYAN "=== SANITIZATION SCHEMES ===" COLOR_RESET "\n");
    for (int i = 0; i < 4; i++) {
        printf(COLOR_CYAN "[%d]" COLOR_RESET " %s\n      " COLOR_WHITE "Reference: %s" COLOR_RESET "\n",
               i + 1, schemes[i].name, schemes[i].standard);
    }
    printf(COLOR_CYAN "Current: [%d] %s\n" COLOR_RESET, current_scheme_idx + 1, schemes[current_scheme_idx].name);
    printf("Enter new scheme (1-4) or 0 to cancel: ");
    int choice;
    if (scanf("%d", &choice) == 1 && choice >= 1 && choice <= 4) {
        current_scheme_idx = choice - 1;
        printf(COLOR_CYAN "[+] Scheme updated.\n" COLOR_RESET);
    }
    while (getchar() != '\n');
}

int main(void) {
    prctl(PR_SET_DUMPABLE, 0);
    struct rlimit rl = {0, 0}; setrlimit(RLIMIT_CORE, &rl);

    startup_compliance_check();

    printf("\n" COLOR_BOLD COLOR_CYAN);
    printf("                    SECURE WIPE v%s\n", VERSION);
    printf(COLOR_RESET);
    printf(COLOR_WHITE "         NIST SP 800-88 Rev. 1 | FIPS 140-3 Aligned\n" COLOR_RESET);

    printf(COLOR_CYAN "         Secure Erase for Controlled Environments\n" COLOR_RESET);
    printf("  ==================================================\n");
    printf(COLOR_WHITE "[i] Runtime compliance warnings disabled. Fallbacks active.\n\n" COLOR_RESET);

    int choice;
    while (1) {
        printf("\n" COLOR_BOLD COLOR_CYAN "=== MAIN MENU ===" COLOR_RESET "\n");
        printf(COLOR_WHITE "1." COLOR_RESET " Sanitize and delete a file\n");
        printf(COLOR_WHITE "2." COLOR_RESET " Sanitize and delete a directory\n");
        printf(COLOR_WHITE "3." COLOR_RESET " Sanitize free space (10MB safe zone)\n");
        printf(COLOR_WHITE "4." COLOR_RESET " Fill RAM (aggressive allocation)\n");
        printf(COLOR_WHITE "5." COLOR_RESET " Release RAM (free all allocated memory)\n");
        printf(COLOR_WHITE "6." COLOR_RESET " Sanitization settings\n");
        printf(COLOR_WHITE "7." COLOR_RESET " Exit\n");
        printf(COLOR_BOLD "Choice: " COLOR_RESET);
        
        if (scanf("%d", &choice) != 1) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { }
            continue;
        }
        while (getchar() != '\n');

        switch (choice) {
            case 1: {
                char path[4096];
                printf("Enter file path: ");
                if (!fgets(path, sizeof(path), stdin)) continue;
                path[strcspn(path, "\n")] = 0;
                if (path[0]) wipe_file(path);
                break;
            }
            case 2: {
                char path[4096];
                printf("Enter directory path: ");
                if (!fgets(path, sizeof(path), stdin)) continue;
                path[strcspn(path, "\n")] = 0;
                if (path[0]) {
                    g_start_time = time(NULL);
                    printf("\n" COLOR_CYAN "[*] Recursively sanitizing directory...\n" COLOR_RESET);
                    wipe_directory_recursive(path);
                    if (rmdir(path) == 0) printf(COLOR_CYAN "[+] Root directory removed.\n" COLOR_RESET);
                    attempt_trim(path);
                }
                break;
            }
            case 3: {
                char path[4096];
                printf("Enter mount point or directory: ");
                if (!fgets(path, sizeof(path), stdin)) continue;
                path[strcspn(path, "\n")] = 0;
                if (path[0]) wipe_free_space(path);
                break;
            }
            case 4: fill_ram(DEFAULT_SAFETY_MB); break;
            case 5: release_ram(); break;
            case 6: show_settings(); break;
            case 7:
                release_ram();  /* free any remaining RAM blocks */
                printf(COLOR_CYAN "[+] Exiting. Memory cleared. Goodbye.\n" COLOR_RESET);
                return EXIT_SUCCESS;
            default: printf(COLOR_BOLD "[!] Invalid option.\n" COLOR_RESET);
        }
    }
    return EXIT_SUCCESS;
}
