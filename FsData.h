#ifndef FSDATA_H
#define FSDATA_H

#include <stdint.h>
#include <cstddef>

#define NAME_SIZE 12
#define MAX_SECTORS_FOR_A_FILE 8 // 4KB file
#define AMOUNT_OF_RESERVED_ITEMS 12 // this 
#define INODE_NAME_OFFSET offsetof(struct inode, name) // Relative to inode struct

struct DirEntry
{
    char name[NAME_SIZE];
    uint32_t inode_number;ge
};

struct inode
{
    uint32_t inode_number;                                // 4
    uint32_t data_locations[MAX_SECTORS_FOR_A_FILE];      // 32

    char name[NAME_SIZE];                                 // 12

    uint16_t file_size;                                   // 2

    uint8_t number_of_sectors;                            // 1
    uint8_t is_dir;                                       // 1

    uint8_t reserved[AMOUNT_OF_RESERVED_ITEMS];           // 12
};

#endif // FSDATA_H