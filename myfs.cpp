#include <string.h>
#include <iostream>
#include <math.h>
#include <sstream>
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <vector>
#include <array>

#include "myfs.h"
#include "file.h"


MyFs::MyFs(BlockDeviceSimulator *blkdevsim_):blkdevsim(blkdevsim_) {
	MyFs::buffer_data_type headers_raw_data = get_sector_data(0);

	struct myfs_header* metadata = reinterpret_cast<myfs_header*>(headers_raw_data.data());
	
	//std::cout << "hello" << metadata->version << std::endl;
	
	if (std::memcmp(metadata->magic, MYFS_MAGIC, sizeof(metadata->magic)) != 0 ||
	    (metadata->version != CURR_VERSION)) {
		std::cout << "Did not find myfs instance on blkdev" << std::endl;
		std::cout << "Creating..." << std::endl;
		format();
		std::cout << "Finished!" << std::endl;
	}
}


void MyFs::format() {
	fill_file_with_null();
	
	insert_fs_headers();

	create_file("/", true);

	for (int i = 0; i < 800; i++)
	{
	 	create_file(std::to_string(i), false);
	}
}


void MyFs::insert_fs_headers()
{
	char buffer[SECTOR_SIZE] = {0};

	myfs_header* header_ptr = reinterpret_cast<myfs_header*>(buffer);

	header_ptr->version = CURR_VERSION;
	header_ptr->sector_size = SECTOR_SIZE;
	header_ptr->bitmap_address = BIT_MAP_ADDRESS;
	header_ptr->inode_table_address = INODE_TABLE_ADDRESS;
	header_ptr->content_address = CONTENT_ADDRESS;

	std::memcpy(header_ptr->magic, MYFS_MAGIC, sizeof(MYFS_MAGIC));
	
	blkdevsim->write(0, buffer);
}


void MyFs::create_file(const std::string& path_str, bool directory)
{
	dir_list dirent = list_dir("/");

	if (is_path_exist(dirent, path_str))
	{
		std::cout << "File already exists\n" << std::endl;
		return;
	}

	if (dirent.size() >= MAX_FILES)
	{
		std::cout << "Failed to create file: Maximum disk capacity reached. Please free space and retry.\n" << std::endl;
		return;
	}

	File new_file = initialize_file(path_str, dirent.size());

	std::array<uint16_t, 2> locations = find_available_inode_sector(dirent.size());
	
	char buffer[SECTOR_SIZE] = {0};

	blkdevsim->read(locations[0], buffer);

	std::memcpy(buffer + locations[1], &new_file._entry, sizeof(inode));

	blkdevsim->write(locations[0], buffer);

	if (directory)
	{
		//throw std::runtime_error("not implemented");
		int x = 1;
	}
}


std::array<uint16_t, 2> MyFs::find_available_inode_sector(uint16_t inode_number)
{
	uint16_t sector_addr = INODE_TABLE_ADDRESS + (inode_number / INODES_PER_SECTOR) * SECTOR_SIZE;
	uint16_t offset = (inode_number % INODES_PER_SECTOR) * sizeof(inode);

	std::array<uint16_t, 2> locations = {}; // location[0] = sector, location[1] = offset

	locations[0] = sector_addr;
	locations[1] = offset;
	return locations;
}


File MyFs::initialize_file(const std::string& path_str, uint16_t inode_number)
{
	File new_file(path_str);
	new_file._entry.inode_number = inode_number;
	std::fill(std::begin(new_file._entry.data_locations), std::end(new_file._entry.data_locations), -1);

	return new_file;
}


File MyFs::find_file(const std::string& path_str, const dir_list& dirent)
{
	for (const File& file : dirent)
	{
		if (std::string(file._entry.name) == path_str) return file;
	}
	throw std::runtime_error("File not found: " + path_str);
}

std::string MyFs::get_content(const std::string& path_str) {
	dir_list dirent = list_dir("/");
	File file = find_file(path_str, dirent);

	std::string ans = "";
	char sector_buffer[SECTOR_SIZE];

	for (size_t i = 0; i < file._entry.number_of_sectors; i++)
	{
		uint32_t addr = CONTENT_ADDRESS + (file._entry.data_locations[i] * SECTOR_SIZE);
		blkdevsim->read(addr, sector_buffer);
		ans.append(sector_buffer, SECTOR_SIZE);
	}

	return ans;
}


std::vector<int> MyFs::write_to_new_sectors(std::string& content)
{
	std::string curr_sector_content;
	std::vector<int> written_sectors;

	while (content.size() != 0)
	{
		if (content.size() >= SECTOR_SIZE)
		{
			curr_sector_content = content.substr(0, SECTOR_SIZE);
			content.erase(0, SECTOR_SIZE);
		}
		else
		{
			curr_sector_content = content;
			content.clear();
		}

		int free_sector_number = find_free_sector();
		written_sectors.push_back(free_sector_number);

		uint32_t addr = CONTENT_ADDRESS + (free_sector_number * SECTOR_SIZE);
		
		blkdevsim->write(addr, curr_sector_content.c_str());
	}
	return written_sectors;
}


std::vector<int> MyFs::append_content_to_file(std::string& content, File& file)
{
	int last_sector = file._entry.data_locations[file._entry.number_of_sectors - 1];
	uint32_t last_sector_addr = CONTENT_ADDRESS + (last_sector * SECTOR_SIZE);

	int remaining_space_in_last_sector = calc_remain_space_in_last_sector(file, last_sector_addr);

	if (remaining_space_in_last_sector >= 0)
	{
		return append_to_last_sector(content, file, last_sector_addr, remaining_space_in_last_sector);
	}
	else
	{
		return write_to_new_sectors(content);
	}
}


std::vector<int> MyFs::append_to_last_sector(std::string& content, File& file, uint32_t last_sector_addr, int remaining_space_in_last_sector)
{
	std::string full_data_sector;
	uint32_t addr = last_sector_addr;
	MyFs::buffer_data_type sector_data = get_sector_data(addr);
	std::string sector_data_str(sector_data.begin(), sector_data.end());

	if (content.size() <= static_cast<std::size_t>(remaining_space_in_last_sector))
	{
		full_data_sector = sector_data_str + content;
		blkdevsim->write(addr, full_data_sector.c_str());
		return {};
	}
	else
	{
		std::string sub_content = content.substr(0, remaining_space_in_last_sector);
		full_data_sector = sector_data_str + sub_content;
	}
	blkdevsim->write(addr, full_data_sector.c_str());
		
	content.erase(0, remaining_space_in_last_sector);
	return write_to_new_sectors(content);
}

MyFs::buffer_data_type MyFs::get_sector_data(uint32_t addr)
{
	MyFs::buffer_data_type sector_data = {0};
	blkdevsim->read(addr, reinterpret_cast<char*>(sector_data.data()));
	return sector_data;
}


int MyFs::calc_remain_space_in_last_sector(const File& file, uint32_t last_sector_addr)
{
	MyFs::buffer_data_type data = get_sector_data(last_sector_addr);

	auto it = std::find(data.begin(), data.end(), '\0');

	if (it == data.end()) return -1;

	int end_content_index = std::distance(data.begin(), it);

	return SECTOR_SIZE - end_content_index;
}


void MyFs::handle_write_content(File& file, std::string& content)
{
	std::vector<int> written_sectors;

	if (file._entry.number_of_sectors == 0)
	{
		written_sectors = write_to_new_sectors(content);
	}
	else
	{
		written_sectors = append_content_to_file(content, file);
	}
	update_file_headers(content, written_sectors, file);
}


void MyFs::set_content(const std::string& path_str, std::string& content) {
	dir_list dirent = list_dir("/");

	if (!is_path_exist(dirent, path_str))
	{
		throw std::runtime_error("File not found: " + path_str);
	}

	// Remove '\n'
	content.pop_back();

	File file = find_file(path_str, dirent);
	handle_write_content(file, content);
}


MyFs::dir_list MyFs::list_dir(const std::string& path_str) {
	uint16_t total_bytes = SECTOR_SIZE * TABLE_SECTORS_AMOUNT;
	uint16_t addr = INODE_TABLE_ADDRESS;
	std::vector<uint8_t> inode_table_buffer(total_bytes, 0);
	uint16_t offset = 0;

	while (addr < CONTENT_ADDRESS)
	{
		MyFs::buffer_data_type curr_sector_buffer = get_sector_data(addr);
		
		if (curr_sector_buffer.at(NAME_LOCATION) == '\0') break; // End of written table.

		std::memcpy(inode_table_buffer.data() + offset, curr_sector_buffer.data(), SECTOR_SIZE);

		offset += SECTOR_SIZE;
		addr += SECTOR_SIZE;
	}
	
	std::vector<inode> inode_vector = map_sector_to_inodes(inode_table_buffer);
	
	uint16_t max_inodes = total_bytes / sizeof(inode);
	dir_list result;

	for (uint16_t i = 0; i < max_inodes; i++)
	{
		if (inode_vector[i].name[0] != 0)
		{
			File curr_file(inode_vector[i]);
			result.push_back(curr_file);
		}
	}

	return result;
}


std::vector<inode> MyFs::map_sector_to_inodes(const std::vector<uint8_t>& buffer)
{
    std::vector<inode> all_inodes;
    uint32_t offset = 0;

    while (offset < buffer.size())
    {
        inode* sector_start = reinterpret_cast<inode*>(const_cast<uint8_t*>(&buffer[offset]));

        for (int i = 0; i < INODES_PER_SECTOR; i++)
        {
            all_inodes.push_back(sector_start[i]);
        }

        offset += SECTOR_SIZE;
    }

    return all_inodes; 
}


void MyFs::fill_file_with_null()
{
	std::vector<char> buffer(SECTOR_SIZE, 0);
	uint32_t addr = 0;

	while (addr != blkdevsim->DEVICE_SIZE)
	{
		blkdevsim->write(addr, buffer.data());
		addr += SECTOR_SIZE;
	}
}


int MyFs::find_inode_number(const std::string& file_name, const dir_list& dirent)
{
	int inode_number = 0;


	for (const File& file : dirent)
	{
		if (std::string(file._entry.name) == file_name)
		{
			return inode_number;
		}
		inode_number++;
	}
	throw std::runtime_error("Error: file not found");
}


int MyFs::find_free_sector()
{
	char buffer[SECTOR_SIZE] = {0};
	uint16_t total_bytes = SECTORES_OF_DATA / 8; 

	blkdevsim->read(BIT_MAP_ADDRESS, buffer);

	for (size_t i = 0; i < total_bytes; i++)
	{
		if (buffer[i] == 255) continue; // all byte(8 sectors) are full.

		for (int bit = 0; bit < 8; bit++)
		{
			if (!((buffer[i] >> bit) & 1))
			{
				buffer[i] |= 1 << bit; 
				blkdevsim->write(BIT_MAP_ADDRESS, buffer);
				
				int sector_num = (i * 8) + bit;
				return sector_num;
			}
		}
	}
	throw std::runtime_error("Error: no free sector");
}


bool MyFs::is_path_exist(const dir_list& dirent, const std::string& file_name)
{
	for (const File& file : dirent)
	{
		if (std::string(file._entry.name) == file_name)
		{
			return true;
		}
	}
	return false;
}


void MyFs::update_file_headers(const std::string& content, const std::vector<int>& sectors, File& file) 
{
	file._entry.file_size += (content.size());
;
	size_t start_idx = file._entry.number_of_sectors;
	for(size_t i = 0; i < sectors.size(); i++) 
	{
		file._entry.data_locations[start_idx + i] = sectors[i];
	}

	file._entry.number_of_sectors += sectors.size();

	write_new_file_metadata(file);
}


void MyFs::write_new_file_metadata(const File& file)
{
	uint16_t sector_addr = INODE_TABLE_ADDRESS + (file._entry.inode_number / INODES_PER_SECTOR);
	uint16_t offset = sizeof(inode) * file._entry.inode_number;

	MyFs::buffer_data_type curr_sector_buffer = get_sector_data(sector_addr);

	std::memcpy(curr_sector_buffer.data() + offset, &file._entry, sizeof(inode));

	blkdevsim->write(sector_addr, reinterpret_cast<char*>(curr_sector_buffer.data()));
}


bool MyFs::is_sector_full(int sector_to_check)
{
	char buffer[SECTOR_SIZE];
	uint32_t addr = CONTENT_ADDRESS + (sector_to_check * SECTOR_SIZE);

	blkdevsim->read(addr, buffer);

	for (size_t i = 0; i < SECTOR_SIZE; i++)
	{
		if (buffer[i] == 0)
		{
			return false;
		}
	}
	return true;
}