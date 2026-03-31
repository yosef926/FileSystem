#ifndef __FILE_H__
#define __FILE_H__

#include <memory>
#include <vector>
#include <stdint.h>
#include "blkdev.h"

#define NAME_SIZE 12
#define MAX_SECTORS_FOR_A_FILE 8 // 4KB file
#define AMOUNT_OF_RESERVED_ITEMS 12 // this 
#define NAME_LOCATION 36 // Relative to inode struct

struct DirEntry
{
    char name[NAME_SIZE];
    uint32_t inode_number;
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

class File {
public:
    File() = default;
    File(std::string name);
    File(inode file_inode);
    friend std::ostream& operator<< (std::ostream& stream, const File& file);

    inode _entry;
};
#endif // __MYFS_H__