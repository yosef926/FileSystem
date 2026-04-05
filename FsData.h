#ifndef FSDATA_H
#define FSDATA_H

#include <stdint.h>
#include <cstddef>
#include <stdint.h>

#include "blkdev.h"


// Structs

static constexpr uint8_t MYFS_MAGIC[4] = { 'M', 'Y', 'F', 'S' };	
#define NAME_SIZE 12
#define MAX_SECTORS_FOR_A_FILE 8 // 4KB file
#define AMOUNT_OF_RESERVED_ITEMS 12


struct myfs_header {
    uint8_t magic[sizeof(MYFS_MAGIC)];
    uint16_t version;
    uint16_t sector_size;
    uint16_t bitmap_content_address;
    uint32_t bitmap_table_address;
    uint32_t inode_table_address;
    uint32_t content_address;
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


// Constants
static constexpr uint16_t CURR_VERSION = 3;
static constexpr uint16_t SECTOR_SIZE = 512;

// Technical numbers
static constexpr uint16_t BITS_IN_SECTOR = SECTOR_SIZE * 8;
static constexpr uint32_t AMOUNT_OF_SECTORS = BlockDeviceSimulator::DEVICE_SIZE / SECTOR_SIZE;
static constexpr uint8_t INODES_PER_SECTOR = SECTOR_SIZE / sizeof(inode);
static constexpr uint8_t ENTRIES_PER_SECTOR = SECTOR_SIZE / sizeof(DirEntry);

// Calculated Addresses
static constexpr uint16_t BITMAP_CONTENT_ADDRESS = SECTOR_SIZE;
static constexpr uint32_t BITMAP_CONTENT_SIZE = BlockDeviceSimulator::DEVICE_SIZE * 0.95 / SECTOR_SIZE / BITS_IN_SECTOR + 1;

static constexpr uint32_t BITMAP_TABLE_ADDRESS = BITMAP_CONTENT_ADDRESS + BITMAP_CONTENT_SIZE;
static constexpr uint32_t BITMAP_TABLE_SIZE = BlockDeviceSimulator::DEVICE_SIZE * 0.05 / sizeof(inode) / BITS_IN_SECTOR + 1;

static constexpr uint32_t INODE_TABLE_ADDRESS = BITMAP_TABLE_ADDRESS + BITMAP_TABLE_SIZE;
static constexpr uint32_t INODE_TABLE_SIZE = (BlockDeviceSimulator::DEVICE_SIZE - SECTOR_SIZE - BITMAP_CONTENT_SIZE - BITMAP_TABLE_SIZE) * 0.05;

static constexpr uint32_t CONTENT_ADDRESS = INODE_TABLE_ADDRESS + INODE_TABLE_SIZE;

#endif // FSDATA_H