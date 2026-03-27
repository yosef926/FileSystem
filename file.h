#ifndef __FILE_H__
#define __FILE_H__

#include <memory>
#include <vector>
#include <stdint.h>
#include "blkdev.h"


#define NAME_SIZE 12
#define MAX_SECTORS_FOR_A_FILE 3 // Enough for roughly a file of 100 words.


class File {
private:
    struct inode {
        char name[NAME_SIZE];
        uint16_t inode_number;
        uint32_t number_of_sectors;
        uint32_t file_size;
        uint8_t is_dir;
        uint32_t data_locations[MAX_SECTORS_FOR_A_FILE];
    };

public:
    File() = default;
    File(std::string name);
    friend std::ostream& operator<< (std::ostream& stream, const File& file);

    inode _entry;
};
#endif // __MYFS_H__