#include "myfs.h"
#include <string.h>
#include <iostream>
#include <math.h>
#include <sstream>
#include <stdint.h>
#include <cstring>
#include "file.h"


File::File(std::string name)
{
    memset(&_entry, 0, sizeof(_entry));

    strncpy(_entry.name, name.c_str(), NAME_SIZE - 1);
    _entry.name[NAME_SIZE - 1] = '\0';
}

File::File(inode file_inode)
{
    _entry = file_inode;
}

std::ostream& operator<< (std::ostream& stream, const File& file)
{
    stream << "name: " << file._entry.name << "\nfile_size: " << file._entry.file_size
    << "\nnumber_of_sectors: " << file._entry.number_of_sectors << "\nis_dir: " << file._entry.is_dir
    << "\ndata_location: [";
    
    for (int i = 0; i < MAX_SECTORS_FOR_A_FILE; ++i) {
        stream << file._entry.data_locations[i];
        
        if (i < MAX_SECTORS_FOR_A_FILE - 1) {
            stream << ", ";
        }
    }
    
    stream << "]";

    return stream;
}