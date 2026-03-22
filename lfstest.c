#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "lfs.h"

#define QSPI_SECTOR_SIZE 4096       // 4 KB block size
#define QSPI_PAGE_SIZE   256        // page size
#define QSPI_SECTORS     (1 * 1024 * 1024 / QSPI_SECTOR_SIZE) // 1 MB total

// Emulated flash in RAM
static uint8_t g_flash[QSPI_SECTOR_SIZE * QSPI_SECTORS];

int disk_read(const struct lfs_config *c, lfs_block_t block,
              lfs_off_t off, void *buffer, lfs_size_t size) {
    memcpy(buffer, &g_flash[block * c->block_size + off], size);
    return 0;
}

int disk_prog(const struct lfs_config *c, lfs_block_t block,
              lfs_off_t off, const void *buffer, lfs_size_t size) {
    memcpy(&g_flash[block * c->block_size + off], buffer, size);
    return 0;
}

int disk_erase(const struct lfs_config *c, lfs_block_t block) {
    memset(&g_flash[block * c->block_size], 0xFF, c->block_size);
    return 0;
}

int disk_sync(const struct lfs_config *c) {
    return 0;
}

// LittleFS configuration
struct lfs_config cfg = {
    .read  = disk_read,
    .prog  = disk_prog,
    .erase = disk_erase,
    .sync  = disk_sync,

    .read_size = QSPI_PAGE_SIZE,
    .prog_size = QSPI_PAGE_SIZE,
    .block_size = QSPI_SECTOR_SIZE,
    .block_count = QSPI_SECTORS,
    .cache_size = QSPI_PAGE_SIZE,
    .lookahead_size = 16,
    .block_cycles = 500,
};

int main() {

    lfs_t lfs;

    // Initialize flash to erased state
    memset(g_flash, 0xFF, sizeof(g_flash));

    // Format
    int res = lfs_format(&lfs, &cfg);
    if (res < 0) {
        printf("Format failed: %d\n", res);
        return 1;
    }
    printf("Formatted\n");

    // Mount
    res = lfs_mount(&lfs, &cfg);
    if (res < 0) {
        printf("Mount failed: %d\n", res);
        return 1;
    }
    printf("Mounted\n");

    // Write
    {
        lfs_file_t file;
        const char *filename = "test.txt";
        const char *data = "Hello LittleFS\n";

        res = lfs_file_open(&lfs, &file, filename, LFS_O_RDWR | LFS_O_CREAT);
        if (res < 0) {
            printf("Open write failed: %d\n", res);
            return 1;
        }

        lfs_file_write(&lfs, &file, data, strlen(data));
        lfs_file_close(&lfs, &file);

        printf("Written to %s\n", filename);
    }

    // Read
    {
        lfs_file_t file;
        const char *filename = "test.txt";
        char buffer[128] = {0};

        res = lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY);
        if (res < 0) {
            printf("Open read failed: %d\n", res);
            return 1;
        }

        lfs_file_read(&lfs, &file, buffer, sizeof(buffer) - 1);
        lfs_file_close(&lfs, &file);

        printf("Read from %s:\n%s\n", filename, buffer);
    }

    // List
    {
        lfs_dir_t dir;
        struct lfs_info info;

        res = lfs_dir_open(&lfs, &dir, "/");
        if (res < 0) {
            printf("Dir open failed: %d\n", res);
            return 1;
        }

        printf("Listing files:\n");
        while (lfs_dir_read(&lfs, &dir, &info) > 0) {
            if (info.type == LFS_TYPE_REG) {
                printf("File: %s, Size: %d bytes\n", info.name, info.size);
            }
        }

        lfs_dir_close(&lfs, &dir);
    }

    // Unmount
    lfs_unmount(&lfs);
    printf("Unmounted\n");

    return 0;
}