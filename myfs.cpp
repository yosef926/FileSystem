#include <string.h>
#include <iostream>
#include <iomanip>
#include <math.h>
#include <sstream>
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <vector>
#include <array>
#include <limits>

#include "myfs.h"
#include "FsData.h"


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


bool MyFs::technical_tests(const inode_list& dirent, const std::string& path_str)
{
	if (dirent.size() >= MAX_FILES)
	{
		std::cout << "Failed to create file: Maximum disk capacity reached. Please free space and retry.\n" << std::endl;
		return false;
	}

	if (is_path_exist(dirent, path_str))
	{
		std::cout << "inode already exists\n" << std::endl;
		return false;
	}

	return true;
}


void MyFs::write_file_to_disk(const inode& file, const inode_list& dirent)
{
	std::array<uint16_t, 2> locations = find_available_inode_sector(dirent.size());  // location[0] = sector, location[1] = offset
	
	char buffer[SECTOR_SIZE] = {0};

	blkdevsim->read(locations[0], buffer);

	std::memcpy(buffer + locations[1], &file, sizeof(inode));

	blkdevsim->write(locations[0], buffer);
}


std::vector<std::string> MyFs::split_path_by_slash(const std::string& path_str)
{
	std::vector<std::string> parts;
    std::stringstream ss(path_str);
    std::string item;

    while (std::getline(ss, item, '/'))
	{
        // Skip empty strings (happens if the path starts with '/' or has '//')
        if (!item.empty())
		{
			parts.push_back(item);
		}
	}

	parts.insert(parts.begin(), "/");
	return parts;
}


std::string MyFs::extract_parent_dir_name(const std::string& path_str)
{
	std::vector<std::string> parts = split_path_by_slash(path_str);

	if (parts.size() == 0)
	{
		throw std::runtime_error("Error: path is not valid");
	}
	else if (parts.size() == 1)
	{
		return "/";
	}

	// Safe: the preceding conditional ensures parts.size() > 1 before reaching this block.
	return parts[parts.size() - 2];
}


void MyFs::create_file(const std::string& path_str, bool directory)
{
	std::string parent_dir_name = extract_parent_dir_name(path_str);
	inode_list dirent = list_root_inodes(parent_dir_name);

	if (!technical_tests(dirent, path_str)) return;

	inode new_file = initialize_inode(path_str, dirent.size(), directory);

	write_file_to_disk(new_file, dirent);
	dirent = list_root_inodes(parent_dir_name);

	if (!directory)
	{
		handle_adding_entry_to_dir(dirent, new_file);
	}
}


void MyFs::handle_adding_entry_to_dir(const inode_list& dirent, const inode& file)
{
	// Should find real parent_dir_name
	std::string parent_dir_name = "/";
	inode parent_inode = find_inode(parent_dir_name, dirent);

	// Should trim '/' before.
	std::string file_name(file.name);
	DirEntry entry = create_dir_entry(file_name, file.inode_number);

	int sector_number = update_parent_inode_metadata(entry, parent_inode);

	write_entry_to_dir(entry, sector_number);
}


DirEntry MyFs::create_dir_entry(const std::string& file_name, const uint32_t& inode_number)
{
	DirEntry entry = {};
	entry.inode_number = inode_number;
	
	std::strncpy(entry.name, file_name.c_str(), sizeof(entry.name) - 1);
	entry.name[sizeof(entry.name) - 1] = '\0';

	return entry;
}


int MyFs::update_parent_inode_metadata(const DirEntry& entry, inode& parent_inode)
{
	int sector_number;

	if (parent_inode.number_of_sectors == 0 || does_dir_last_sector_full(parent_inode))
	{
		sector_number = find_free_sector();
		parent_inode.number_of_sectors++;
		parent_inode.data_locations[parent_inode.number_of_sectors - 1] = sector_number;
	}
	else
	{
		sector_number = parent_inode.data_locations[parent_inode.number_of_sectors - 1];
	}

	update_inode_table(parent_inode);

	return sector_number;
}


bool MyFs::does_dir_last_sector_full(const inode& parent_dir)
{
	int last_sector_addr = parent_dir.data_locations[parent_dir.number_of_sectors - 1];

	MyFs::buffer_data_type buffer = get_sector_data(last_sector_addr);

	if (buffer.at(SECTOR_SIZE - sizeof(DirEntry)) != '\0') return true;
	return false;
}


void MyFs::write_entry_to_dir(const DirEntry& entry, int sector_number)
{
	uint16_t target_addr = CONTENT_ADDRESS + (sector_number * SECTOR_SIZE);

	MyFs::buffer_data_type sector_buffer = get_sector_data(target_addr);

	DirEntry* dirEntry_array = reinterpret_cast<DirEntry*>(sector_buffer.data());

	uint16_t offset = calc_offset_for_dirEntry(dirEntry_array);
	
	dirEntry_array[offset] = entry;

	blkdevsim->write(target_addr, reinterpret_cast<char*>(dirEntry_array));
}


int MyFs::calc_offset_for_dirEntry(DirEntry* dirEntry_array)
{
	uint8_t max_entries_in_sector = SECTOR_SIZE / sizeof(DirEntry);

	for (int i = 0; i < max_entries_in_sector; i++)
	{
		if (dirEntry_array[i].inode_number == 0)
		{
			return i;
		}
	}
	throw std::runtime_error("Error: No offset for new entry was found\n");
}


void MyFs::update_inode_table(const inode& new_inode)
{
	uint16_t sector_addr = INODE_TABLE_ADDRESS + (new_inode.inode_number / INODES_PER_SECTOR);

	MyFs::buffer_data_type sector_inode_table = get_sector_data(sector_addr);
	
	inode* inodes_array = reinterpret_cast<inode*>(sector_inode_table.data());

	inodes_array[new_inode.inode_number] = new_inode;

	blkdevsim->write(sector_addr, reinterpret_cast<char*>(inodes_array));
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


inode MyFs::initialize_inode(const std::string& path_str, uint16_t inode_number, uint8_t is_dir)
{
	inode new_inode = {0};

	std::strncpy(new_inode.name, path_str.c_str(), sizeof(new_inode.name) - 1);
	new_inode.name[sizeof(new_inode.name) - 1] = '\0';
	new_inode.inode_number = inode_number;
	new_inode.is_dir = is_dir;
	std::fill(std::begin(new_inode.data_locations), std::end(new_inode.data_locations), -1);

	return new_inode;
}


inode MyFs::find_inode(const std::string& path_str, const inode_list& dirent)
{
	for (const inode& file : dirent)
	{
		if (std::string(file.name) == path_str) return file;
	}
	throw std::runtime_error("inode not found: " + path_str);
}

std::string MyFs::get_content(const std::string& path_str) {
	inode_list dirent = list_root_entries();
	inode file = find_inode(path_str, dirent);

	std::string ans = "";
	char sector_buffer[SECTOR_SIZE];

	for (size_t i = 0; i < file.number_of_sectors; i++)
	{
		uint32_t addr = CONTENT_ADDRESS + (file.data_locations[i] * SECTOR_SIZE);
		blkdevsim->read(addr, sector_buffer);
		ans.append(sector_buffer, SECTOR_SIZE);
	}

	return ans;
}


std::vector<int> MyFs::write_to_new_sectors(std::string content)
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
		MyFs::buffer_data_type sector_vector = get_sector_data(addr);

		std::copy(curr_sector_content.begin(), curr_sector_content.begin() + curr_sector_content.size(), sector_vector.begin());

		blkdevsim->write(addr, reinterpret_cast<char*>(sector_vector.data()));
	}
	return written_sectors;
}


std::vector<int> MyFs::append_content_to_file(std::string content, inode& file)
{
	int last_sector = file.data_locations[file.number_of_sectors - 1];
	uint32_t last_sector_addr = CONTENT_ADDRESS + (last_sector * SECTOR_SIZE);

	int remaining_space_in_last_sector = calc_remain_space_in_last_sector(file, last_sector_addr);

	if (remaining_space_in_last_sector > 0)
	{
		return append_to_last_sector(content, file, last_sector_addr, remaining_space_in_last_sector);
	}
	else
	{
		return write_to_new_sectors(content);
	}
}


std::vector<int> MyFs::append_to_last_sector(std::string& content, inode& file, uint32_t last_sector_addr, int remaining_space_in_last_sector)
{
	MyFs::buffer_data_type sector_data = get_sector_data(last_sector_addr);
	uint32_t offset = SECTOR_SIZE - remaining_space_in_last_sector;

	size_t bytes_to_copy = std::min<size_t>(content.size(), remaining_space_in_last_sector);

	std::copy(content.begin(), content.begin() + bytes_to_copy, sector_data.begin() + offset);
	
	blkdevsim->write(last_sector_addr, reinterpret_cast<char*>(sector_data.data()));

	if (content.size() > static_cast<size_t>(remaining_space_in_last_sector))
	{
        std::string leftovers = content.substr(bytes_to_copy); 
        return write_to_new_sectors(leftovers);
	}
	return {};
}


MyFs::buffer_data_type MyFs::get_sector_data(uint32_t addr)
{
	MyFs::buffer_data_type sector_data = {0};
	blkdevsim->read(addr, reinterpret_cast<char*>(sector_data.data()));
	return sector_data;
}


int MyFs::calc_remain_space_in_last_sector(const inode& file, uint32_t last_sector_addr)
{
	MyFs::buffer_data_type data = get_sector_data(last_sector_addr);

	auto it = std::find(data.begin(), data.end(), '\0');

	if (it == data.end()) return -1;

	int end_content_index = std::distance(data.begin(), it);

	return SECTOR_SIZE - end_content_index;
}


void MyFs::handle_write_content(DirEntry& file, std::string& content)
{
	std::vector<int> written_sectors;

	if (file.file_size == 0)
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
	inode_list dirent = list_root_inodes("/");

	if (!is_path_exist(dirent, path_str))
	{
		throw std::runtime_error("inode not found: " + path_str);
	}

	content.push_back('\0');

	inode file = find_inode(path_str, dirent);
	handle_write_content(file, content);
}


MyFs::dir_entry_list MyFs::list_root_entries()
{
	std::string file_name = "/";
	inode_list buffer;

	blkdevsim->read(INODE_TABLE_ADDRESS, reinterpret_cast<char*>(buffer.data()));

	inode root_inode = buffer.at(0);
	return get_entries_under_dir(root_inode);
}


MyFs::dir_entry_list MyFs::get_entries_under_dir(const inode& inode)
{
	dir_entry_list entries;
	dir_entry_list curr_sector_entries;
	uint32_t addr;

	for (int i = 0; i < MAX_SECTORS_FOR_A_FILE; i++)
	{
		if (inode.data_locations[i] != std::numeric_limits<uint32_t>::max())
		{
			addr = CONTENT_ADDRESS + SECTOR_SIZE * inode.data_locations[i];
			blkdevsim->read(addr, reinterpret_cast<char*>(curr_sector_entries.data()));
			for (const auto& dir_entry : curr_sector_entries)
			{
				if (dir_entry.name[0] != 0) entries.push_back(dir_entry);
				else break;
			}
		}
		else break;
	}
	return entries;
}


/*
MyFs::dir_entry_list MyFs::list_root_inodes(const std::string& path_str) {
	uint16_t total_bytes = SECTOR_SIZE * TABLE_SECTORS_AMOUNT;
	uint16_t addr = INODE_TABLE_ADDRESS;
	std::vector<uint8_t> buffer(total_bytes, 0);
	uint16_t offset = 0;

	while (addr < CONTENT_ADDRESS)
	{
		MyFs::buffer_data_type curr_sector_buffer = get_sector_data(addr);
		
		if (curr_sector_buffer.at(INODE_NAME_OFFSET) == '\0') break; // End of written table.

		std::memcpy(buffer.data() + offset, curr_sector_buffer.data(), SECTOR_SIZE);

		offset += SECTOR_SIZE;
		addr += SECTOR_SIZE;
	}
	
	std::vector<inode> inode_vector = map_sector_to_inodes(buffer);

	return inode_vector;
}
*/


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


int MyFs::find_inode_number(const std::string& file_name, const inode_list& dirent)
{
	int inode_number = 0;


	for (const inode& file : dirent)
	{
		if (std::string(file.name) == file_name)
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


bool MyFs::is_path_exist(const inode_list& dirent, const std::string& file_name)
{
	for (const inode& file : dirent)
	{
		if (std::string(file.name) == file_name)
		{
			return true;
		}
	}
	return false;
}


void MyFs::update_file_headers(const std::string& content, const std::vector<int>& sectors, inode& file) 
{
	file.file_size += (content.size());
;
	size_t start_idx = file.number_of_sectors;
	for(size_t i = 0; i < sectors.size(); i++) 
	{
		file.data_locations[start_idx + i] = sectors[i];
	}

	file.number_of_sectors += sectors.size();
	write_new_file_metadata(file);
}


void MyFs::write_new_file_metadata(const inode& file)
{
	uint16_t sector_addr = INODE_TABLE_ADDRESS + (file.inode_number / INODES_PER_SECTOR);
	uint16_t offset = sizeof(inode) * file.inode_number;

	MyFs::buffer_data_type curr_sector_buffer = get_sector_data(sector_addr);

	std::memcpy(curr_sector_buffer.data() + offset, &file, sizeof(inode));

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


int MyFs::resolve_path(const std::vector<std::string>& parts)
{
	dir_entry_list curr_dirent = list_root_entries();
	inode curr_inode;
	bool found = false;

	for (int i = 0; i < parts.size(); i++)
	{
		for (int j = 0; j < curr_dirent.size(); j++)
		{
			if (parts.at(i) == std::string(curr_dirent.at(j).name))
			{
				curr_inode = get_inode(curr_dirent.at(j).inode_number);
				curr_dirent = get_entries_under_dir(curr_inode);
				found = true; 
			}
		}
		if(!found)
		{
			throw std::runtime_error("Path error: one of the files do not exist");
		}
		found = false;
	}
}


inode MyFs::get_inode(uint32_t& inode_number)
{
	inode_list inodes;
	uint32_t addr = INODE_TABLE_ADDRESS + (inode_number / INODES_PER_SECTOR) * SECTOR_SIZE;
	
	blkdevsim->read(addr, reinterpret_cast<char*>(inodes.data()));

	return inodes.at(inode_number % INODES_PER_SECTOR);
}


MyFs::dir_entry_list MyFs::list_dir_entries(const std::string& path_str)
{
	std::vector<std::string> parts = split_path_by_slash(path_str);

	int parent_dir_inode_number = resolve_path(parts);
	
	if (parent_dir_inode_number == -1)
	{
		throw std::runtime_error("Path is not valid: " + path_str);
	}
	
	// Code
}