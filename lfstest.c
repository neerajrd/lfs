#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "lfs.h"

#define NO_OF_FILES 4

#define COUNTER_DIR "/counter"

// Global variables for file name and size
char g_filenames[NO_OF_FILES][256] = {"1MB.bin", "2MB.bin", "3MB.bin", "4MB.bin"};
size_t g_filesizes[NO_OF_FILES] = {1 * 1024 * 1024, 2 * 1024 * 1024, 3 * 1024 * 1024, 4 * 1024 * 1024}; // Sizes in bytes
int g_fileopen[NO_OF_FILES];

int g_file_index = 0; // Default index

#define QSPI_SECTOR_SIZE (256 * 1024) // 256 KB
#define QSPI_PAGE_SIZE 4096
#define QSPI_SECTORS  (256 * 1024 * 1024 / QSPI_SECTOR_SIZE) // Calculated block count

uint8_t g_bad_blocks[QSPI_SECTORS];

// Disk file descriptor
int disk_fd;

// Global variable for file operations
lfs_file_t g_files[NO_OF_FILES];

int disk_read(const struct lfs_config *c, lfs_block_t block,
              lfs_off_t off, void *buffer, lfs_size_t size) {

    if (g_bad_blocks[block]) {
        printf("Bad block access (read) %d\n", block);
        return LFS_ERR_CORRUPT;
    }

    lseek(disk_fd, block * c->block_size + off, SEEK_SET);
    read(disk_fd, buffer, size);
    return 0;
}

int disk_prog(const struct lfs_config *c, lfs_block_t block,
              lfs_off_t off, const void *buffer, lfs_size_t size) {
    
    if (g_bad_blocks[block]) {
        printf("Bad block access (prog) %d\n", block);
        return LFS_ERR_CORRUPT;
    }

    printf(">>> %d %d %d\n", block, off, size);
    lseek(disk_fd, block * c->block_size + off, SEEK_SET);
    write(disk_fd, buffer, size);
    return 0;
}

int disk_erase(const struct lfs_config *c, lfs_block_t block) {

    if (g_bad_blocks[block]) {
        printf("Bad block access (erase) %d\n", block);
        return LFS_ERR_CORRUPT;
    }

    uint8_t data[QSPI_SECTOR_SIZE];
    memset(data, 0xFF, QSPI_SECTOR_SIZE); // Ensure the block is filled with 0xFF

    lseek(disk_fd, block * c->block_size, SEEK_SET);
    ssize_t written = write(disk_fd, data, QSPI_SECTOR_SIZE);
    if (written != QSPI_SECTOR_SIZE) {
        perror("Error writing to disk during erase");
        return -1;
    }

    return 0;
}

int disk_sync(const struct lfs_config *c) {
    fsync(disk_fd);
    return 0;
}

// LittleFS configuration
struct lfs_config cfg = {
    // block device operations
    .read  = disk_read,
    .prog  = disk_prog,
    .erase = disk_erase,
    .sync  = disk_sync,

    // block device configuration
    .read_size = QSPI_PAGE_SIZE,
    .prog_size = QSPI_PAGE_SIZE,
    .block_size = QSPI_SECTOR_SIZE,
    .block_count = QSPI_SECTORS,
    .cache_size = QSPI_PAGE_SIZE,
    .lookahead_size = QSPI_PAGE_SIZE,
    .block_cycles = 500,
};

void print_menu() {
    printf("\nMenu:\n");
    printf("q. Exit\n");

    printf("m. Mount filesystem\n");
    printf("u. Unmount filesystem\n");
    printf("f. Format filesystem\n");

    printf("w. Write to file\n");
    printf("r. Read from file\n");
    printf("l. List files\n");
    printf("d. Delete file\n");
    printf("s. Set file index (0-%d)\n", NO_OF_FILES - 1);

    printf("e. Erase block\n");
    printf("b. Mark block as bad\n");    
    printf("c. Corrupt block\n");

    printf("1..9. Tests\n");
}

volatile int g_loop_cont = 0;

void handler(int sig) {
    g_loop_cont = 0;
}

int main() {

    lfs_t lfs;

    signal(SIGQUIT, handler);   // Ctrl+\

    // Open the block device
    disk_fd = open("/dev/sda", O_RDWR);
    if (disk_fd < 0) {
        perror("Failed to open block device");
        return 1;
    }

    print_menu();

    char action = 'm';
    while (1) {

        switch (action) {

            case 'q': {
                // Exit
                lfs_unmount(&lfs);
                close(disk_fd);
                return 0;
            }

            case 'h': {
                // Help
                print_menu();
                break;
            }

            case 'm': {
                // Mount filesystem
                int res = lfs_mount(&lfs, &cfg);
                if (res < 0) {
                    printf("Error mounting filesystem: %d\n", res);
                } else {
                    printf("Filesystem mounted successfully\n");
                }
                break;
            }

            case 'u': {
                // Unmount filesystem
                int res = lfs_unmount(&lfs);
                if (res < 0) {
                    printf("Error unmounting filesystem: %d\n", res);
                } else {
                    printf("Filesystem unmounted successfully\n");
                }
                break;
            }

            case 'f': {
                // Format filesystem
                int res = lfs_format(&lfs, &cfg);
                if (res < 0) {
                    printf("Error formatting filesystem: %d\n", res);
                } else {
                    printf("Filesystem formatted successfully\n");
                }
                break;
            }

            case 'w': {
                // Check if file handle is open
                if (g_fileopen[g_file_index]) {
                    printf("Closing the file first %s\n", g_filenames[g_file_index]);
                    lfs_file_close(&lfs, &g_files[g_file_index]);
                    g_fileopen[g_file_index] = 0;
                }

                // Write to file
                int res = lfs_file_open(&lfs, &g_files[g_file_index], g_filenames[g_file_index], LFS_O_RDWR | LFS_O_CREAT);
                if (res < 0) {
                    printf("Error opening file %s for writing: %d\n", g_filenames[g_file_index], res);
                    break;
                }
                g_fileopen[g_file_index] = 1;

                uint32_t value = 0;
                for (size_t i = 0; i < g_filesizes[g_file_index] / sizeof(uint32_t); i++) {
                    lfs_file_write(&lfs, &g_files[g_file_index], &value, sizeof(uint32_t));
                    value++;
                }

                lfs_file_close(&lfs, &g_files[g_file_index]);
                g_fileopen[g_file_index] = 0;
                printf("Data written to %s\n", g_filenames[g_file_index]);
                break;
            }

            case 'r': {
                // Check if file handle is open
                if (g_fileopen[g_file_index]) {
                    printf("Closing the file first %s\n", g_filenames[g_file_index]);
                    lfs_file_close(&lfs, &g_files[g_file_index]);
                    g_fileopen[g_file_index] = 0;
                }

                // Read from file
                int res = lfs_file_open(&lfs, &g_files[g_file_index], g_filenames[g_file_index], LFS_O_RDONLY);
                if (res < 0) {
                    printf("Error opening file %s for reading: %d\n", g_filenames[g_file_index], res);
                    break;
                }
                g_fileopen[g_file_index] = 1;

                uint32_t value = 0;
                uint32_t read_value;
                for (size_t i = 0; i < g_filesizes[g_file_index] / sizeof(uint32_t); i++) {
                    ssize_t bytes_read = lfs_file_read(&lfs, &g_files[g_file_index], &read_value, sizeof(uint32_t));
                    if (bytes_read < 0 || bytes_read != sizeof(uint32_t) || read_value != value) {
                        printf("\r\nError or data verification failed at index %zu\n", i);
                        break;
                    }
                    value++;
                }

                lfs_file_close(&lfs, &g_files[g_file_index]);
                g_fileopen[g_file_index] = 0;
                printf("Data verification successful for %s\n", g_filenames[g_file_index]);
                break;
            }

            case 'l': {
                // List files
                lfs_dir_t dir;
                struct lfs_info info;
                int res = lfs_dir_open(&lfs, &dir, "/");
                if (res < 0) {
                    printf("Error opening directory: %d\n", res);
                    break;
                }

                printf("Listing files:\n");
                while (lfs_dir_read(&lfs, &dir, &info) > 0) {
                    if (info.type == LFS_TYPE_REG) {
                        printf("File: %s, Size: %d bytes\n", info.name, info.size);
                    }
                }

                lfs_dir_close(&lfs, &dir);
                break;
            }

            case 'd': {
                // Delete file
                int res = lfs_remove(&lfs, g_filenames[g_file_index]);
                if (res < 0) {
                    printf("Error deleting file %s: %d\n", g_filenames[g_file_index], res);
                } else {
                    printf("File %s deleted successfully\n", g_filenames[g_file_index]);
                }
                break;
            }

            case 's': {
                // Set file index
                printf("Enter file index (0-%d): ", NO_OF_FILES - 1);
                int index;
                if (scanf("%d", &index) == 1 && index >= 0 && index < NO_OF_FILES) {
                    g_file_index = index;
                    printf("Selected file: %s, Size: %zu bytes\n", g_filenames[g_file_index], g_filesizes[g_file_index]);
                } else {
                    printf("Invalid file index.\n");
                }
                break;
            }

            case 'e': {
                // Erase block
                printf("Enter the block number to erase: ");
                uint32_t block_num;
                if (scanf("%u", &block_num) == 1) {
                    int res = disk_erase(&cfg, block_num);
                    if (res == 0) {
                        printf("Block %u erased successfully\n", block_num);
                    } else {
                        printf("Error erasing block %u: %d\n", block_num, res);
                    }
                } else {
                    printf("Invalid block number.\n");
                }
                break;
            }

            case 'b': {
                printf("Enter block number to mark as bad: ");

                uint32_t block;
                if (scanf("%u", &block) == 1 && block < QSPI_SECTORS) {
                    g_bad_blocks[block] = 1;
                    printf("Block %u marked as BAD\n", block);
                } else {
                    printf("Invalid block number\n");
                }

                break;
            }

            case 'c': {
                // Corrupt block

                printf("Enter block number to corrupt: ");

                uint32_t block;
                if (scanf("%u", &block) != 1 || block >= QSPI_SECTORS) {
                    printf("Invalid block number\n");
                    break;
                }

                uint8_t data[256];

                for (size_t i = 0; i < sizeof(data); i++) {
                    data[i] = rand() & 0xFF;
                }

                lseek(disk_fd, block * QSPI_SECTOR_SIZE, SEEK_SET);

                ssize_t written = write(disk_fd, data, sizeof(data));
                if (written != sizeof(data)) {
                    perror("Error corrupting block");
                } else {
                    printf("Block %u corrupted at beginning\n", block);
                }

                break;
            }


            case '1': {
                uint32_t value = 0;
                lfs_file_t file;
                char filename[64];

                uint8_t dummy[1019];
                memset(dummy, 0xAA, sizeof(dummy));

                // Make sub-directory

#ifdef          COUNTER_DIR

                lfs_mkdir(&lfs, COUNTER_DIR);
#endif
                // Read existing value if file exists

#ifdef          COUNTER_DIR

                snprintf(filename, sizeof(filename), COUNTER_DIR "/counter00.bin");
#else
                snprintf(filename, sizeof(filename), "counter00.bin");
#endif
                int res = lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY);
                if (res >= 0) {
                    uint32_t existing;
                    ssize_t bytes = lfs_file_read(&lfs, &file, &existing, sizeof(existing));
                    if (bytes == sizeof(existing)) {
                        value = existing + 1;
                    } else {
                        printf("Error reading existing counter from %s\n", filename);
                    }
                    lfs_file_close(&lfs, &file);
                }

                g_loop_cont = 1;
                while (g_loop_cont) {
                    for (int i = 0; i < 1; i++) {

#ifdef                  COUNTER_DIR

                        snprintf(filename, sizeof(filename), COUNTER_DIR "/counter%02d.bin", i);
#else
                        snprintf(filename, sizeof(filename), "counter%02d.bin", i);
#endif
                        int res = lfs_file_open(&lfs, &file, filename, LFS_O_RDWR | LFS_O_CREAT);
                        if (res < 0) {
                            printf("Error opening file %s: %d\n", filename, res);
                            continue;
                        }

                        int written = lfs_file_write(&lfs, &file, &value, sizeof(value));
                        if (written < 0) {
                            printf("Error writing to file %s: %d\n", filename, written);
                            lfs_file_close(&lfs, &file);
                            continue;
                        }

                        written = lfs_file_write(&lfs, &file, dummy, sizeof(dummy));
                        if (written < 0) {
                            printf("Error writing dummy bytes to file %s: %d\n", filename, written);
                            lfs_file_close(&lfs, &file);
                            continue;
                        }

                        lfs_file_close(&lfs, &file);

                        printf("Written value %u to %s\n", value, filename);
                    }

                    value++;

                    usleep(500 * 1000);
                }

                break;
            }

            case '2': {
                uint32_t value;
                lfs_file_t file;
                char filename[64];

#ifdef          COUNTER_DIR

                snprintf(filename, sizeof(filename), COUNTER_DIR "/counter00.bin");
#else
                snprintf(filename, sizeof(filename), "counter00.bin");
#endif
                int res = lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY);

                if (res < 0) {
                    printf("Error opening file /counter/counter01.bin: %d\n", res);
                    break;
                }

                ssize_t bytes = lfs_file_read(&lfs, &file, &value, sizeof(value));
                if (bytes == sizeof(value)) {
                    printf("Counter value: %u\n", value);
                } else {
                    printf("Error reading counter\n");
                }

                lfs_file_close(&lfs, &file);

                break;
            }

            case '3': {

#ifdef          LITTLEFS_VER_2_11_2

                // Make filesystem consistent

                int res = lfs_fs_mkconsistent(&lfs);

                if (res < 0) {
                    printf("Error making filesystem consistent: %d\n", res);
                } else {
                    printf("Filesystem made consistent\n");
                }
#endif
                break;
            }            

            case '4': {
                // Trigger LFS assert by closing file twice

                lfs_file_t file;
                const char *filename = "assert_test.bin";

                int res = lfs_file_open(&lfs, &file, filename, LFS_O_RDWR | LFS_O_CREAT);
                if (res < 0) {
                    printf("Error opening file %s: %d\n", filename, res);
                    break;
                }

                printf("Closing file first time\n");
                lfs_file_close(&lfs, &file);

                printf("Closing file second time\n");
                lfs_file_close(&lfs, &file);

                break;
            }

            default:
                printf("Invalid input. Please enter a letter from the menu.\n");
        }

        printf("-----------------------------------------------------------\n");
        printf("Enter your choice: ");
        scanf(" %c", &action); // Add space to consume any whitespace
    }

    return 0;
}
