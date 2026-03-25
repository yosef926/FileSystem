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