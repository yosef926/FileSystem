#ifndef FSDATA_H
#define FSDATA_H

#include <stdint.h>
#include <cstddef>

#include "blkdev.h"

#define NAME_SIZE 12
#define MAX_SECTORS_FOR_A_FILE 8 // 4KB file
#define AMOUNT_OF_RESERVED_ITEMS 12
#define INODE_NAME_OFFSET offsetof(struct inode, name) // Relative to inode struct

struct myfs_header {
    uint8_t magic[sizeof(MYFS_MAGIC)];
    uint16_t version;
    uint16_t sector_size;
    uint16_t bitmap_table_address;
    uint16_t bitmap_content_address;
    uint16_t inode_table_address;
    uint16_t content_address;
};


struct DirEntry
{
    uint8_t name[NAME_SIZE];                              // 12

    uint32_t inode_number;                                // 4

    uint16_t file_size;                                   // 2

    uint16_t is_dir;                                      // 2

    uint8_t reserved[AMOUNT_OF_RESERVED_ITEMS];           // 12
};

struct inode
{
    uint32_t data_locations[MAX_SECTORS_FOR_A_FILE];      // 32
};

constexpr uint16_t CURR_VERSION = 3;
constexpr uint16_t SECTOR_SIZE = 512;
constexpr uint32_t AMOUNT_OF_SECTORS = BlockDeviceSimulator::DEVICE_SIZE / SECTOR_SIZE;
constexpr uint32_t SECTORES_OF_DATA = (AMOUNT_OF_SECTORS - 3) * 0.95; // 3 is (headers + 2*bitmap) sectors
static constexpr uint16_t TABLE_SECTORS_AMOUNT = (AMOUNT_OF_SECTORS - 3) * 0.05;
static constexpr uint8_t INODES_PER_SECTOR = SECTOR_SIZE / sizeof(inode);
static constexpr uint8_t ENTRIES_PER_SECTOR = SECTOR_SIZE / sizeof(DirEntry);
static constexpr uint16_t MAX_FILES = TABLE_SECTORS_AMOUNT * INODES_PER_SECTOR;

// Calculated Addresses
static constexpr uint16_t BITMAP_CONTENT_ADDRESS = SECTOR_SIZE;
static constexpr uint16_t BITMAP_TABLE_ADDRESS = BITMAP_CONTENT_ADDRESS + SECTOR_SIZE;
static constexpr uint32_t INODE_TABLE_ADDRESS = BITMAP_TABLE_ADDRESS + SECTOR_SIZE;
static constexpr uint32_t CONTENT_ADDRESS = INODE_TABLE_ADDRESS + TABLE_SECTORS_AMOUNT * SECTOR_SIZE;

static constexpr uint8_t MYFS_MAGIC[4] = { 'M', 'Y', 'F', 'S' };	

#endif // FSDATA_H