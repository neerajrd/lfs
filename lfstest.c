#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "lfs.h"

#define QSPI_SECTOR_SIZE (256 * 1024) // 256 KB
#define QSPI_PAGE_SIZE 4096
#define QSPI_SECTORS  (256 * 1024 * 1024 / QSPI_SECTOR_SIZE)

int disk_fd;

int disk_read(const struct lfs_config *c, lfs_block_t block,
              lfs_off_t off, void *buffer, lfs_size_t size) {

    lseek(disk_fd, block * c->block_size + off, SEEK_SET);
    read(disk_fd, buffer, size);
    return 0;
}

int disk_prog(const struct lfs_config *c, lfs_block_t block,
              lfs_off_t off, const void *buffer, lfs_size_t size) {
    
    lseek(disk_fd, block * c->block_size + off, SEEK_SET);
    write(disk_fd, buffer, size);
    return 0;
}

int disk_erase(const struct lfs_config *c, lfs_block_t block) {

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
}

int main() {

    lfs_t lfs;

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
                // Write to file
                lfs_file_t file;
                const char *filename = "test.txt";
                const char *data = "Hello LittleFS\n";

                int res = lfs_file_open(&lfs, &file, filename, LFS_O_RDWR | LFS_O_CREAT);
                if (res < 0) {
                    printf("Error opening file %s for writing: %d\n", filename, res);
                    break;
                }

                lfs_file_write(&lfs, &file, data, strlen(data));

                lfs_file_close(&lfs, &file);
                printf("Written to %s\n", filename);
                break;
            }

            case 'r': {
                // Read from file
                lfs_file_t file;
                const char *filename = "test.txt";
                char buffer[128] = {0};

                int res = lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY);
                if (res < 0) {
                    printf("Error opening file %s for reading: %d\n", filename, res);
                    break;
                }
                lfs_file_read(&lfs, &file, buffer, sizeof(buffer) - 1);
                lfs_file_close(&lfs, &file);

                printf("Read from %s:\n%s\n", filename, buffer);
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
                const char *filename = "test.txt";
                // Delete file
                int res = lfs_remove(&lfs, filename);
                if (res < 0) {
                    printf("Error deleting file %s: %d\n", filename, res);
                } else {
                    printf("File %s deleted successfully\n", filename);
                }
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
