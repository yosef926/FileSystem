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
#include <unistd.h>

#include "myfs.h"
#include "FsData.h"


MyFs::MyFs(BlockDeviceSimulator *blkdevsim_):blkdevsim(blkdevsim_)
{
	MyFs::buffer_data_type headers_raw_data = get_sector_data(0);

	struct myfs_header* metadata = reinterpret_cast<myfs_header*>(headers_raw_data.data());
	
	
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
	create_file("file", false);

	std::string content =
						  "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
						  "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
						  "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
						  "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
						  "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
						  "123456789012";

	for (int i = 0; i < MAX_SECTORS_FOR_A_FILE; i++)
	{
		std::string var = "file";
		set_content(var, content);
	}
}


void MyFs::insert_fs_headers()
{
	char buffer[SECTOR_SIZE] = {0};

	myfs_header* header_ptr = reinterpret_cast<myfs_header*>(buffer);

	header_ptr->version = CURR_VERSION;
	header_ptr->sector_size = SECTOR_SIZE;
	header_ptr->bitmap_content_address = BITMAP_CONTENT_ADDRESS;
	header_ptr->bitmap_table_address = BITMAP_TABLE_ADDRESS;
	header_ptr->inode_table_address = INODE_TABLE_ADDRESS;
	header_ptr->content_address = CONTENT_ADDRESS;

	std::memcpy(header_ptr->magic, MYFS_MAGIC, sizeof(MYFS_MAGIC));
	
	blkdevsim->write(0, buffer);
}


void MyFs::write_new_inode(const uint32_t& inode_number, const inode& new_inode)
{
	std::array<uint32_t, 2> locations = calc_inode_write_locations(inode_number); // location[0] = sector, location[1] = offset

	char buffer[SECTOR_SIZE] = {0};

	blkdevsim->read(locations[0], buffer);

	std::memcpy(buffer + locations[1], &new_inode, sizeof(inode));

	blkdevsim->write(locations[0], buffer);
}


std::vector<std::string> MyFs::split_path_by_slash(const std::string& path)
{
	std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;

    while (std::getline(ss, item, '/'))
	{
        // Skip empty strings (happens if the path starts with '/' or has '//')
        if (!item.empty())
		{
			parts.push_back(item);
		}
	}
	return parts;
}


std::string MyFs::get_parent_path(const std::string& path)
{
	if (path.empty()) return "";

    size_t last_slash_idx = path.find_last_of('/');

    if (last_slash_idx == std::string::npos || last_slash_idx == 0) {
        return "/"; 
    }
    return path.substr(0, last_slash_idx);
}


std::string MyFs::get_file_name_from_path(const std::string& path)
{
	if (path == "/") return path;

	size_t last_slash_idx = path.find_last_of('/');

    if (last_slash_idx == std::string::npos) {
        return path; 
    }
    return path.substr(last_slash_idx+1);
}


void MyFs::does_file_exists(const dir_entry_list& parent_entries, const std::string& file_name)
{
	for (const DirEntry& dir_entry : parent_entries)
	{
		if (std::memcmp(dir_entry.name, file_name.c_str(), file_name.size()) == 0)
		{
			throw std::runtime_error("Error: A file with this name already exists in this folder\n");
		}
	}
}


void MyFs::does_inode_table_full()
{
	uint32_t addr = CONTENT_ADDRESS - SECTOR_SIZE;
	
	buffer_data_type sector_data = get_sector_data(addr);

	if (sector_data.at(SECTOR_SIZE - sizeof(inode)) != '\0')
	{
		throw std::runtime_error("Error: disk has reached max files");
	}
}


uint32_t MyFs::pre_create_checks(const std::string& path)
{
	std::string parent_path = get_parent_path(path);

	// check 1
	does_inode_table_full();

	// Check 2 - if ls_command not crash means all files in path are really folders(except last one which is file OR folder)
	dir_entry_list parent_entries = ls_command(parent_path);

	// Check 3 - if this method throw error means no free sector is available for a new file
	uint32_t inode_number = search_free_bit(BITMAP_TABLE_ADDRESS, BITMAP_TABLE_SECTORS_REQUIRED);

	//check 4
	std::string file_name = get_file_name_from_path(path);
	if (file_name.size() >= NAME_SIZE)
	{
    	throw std::runtime_error("Error: Filename too long!");
	}

	//check 5
	does_file_exists(parent_entries, file_name);
	
	return inode_number;
}


inode MyFs::initialize_inode()
{
	inode new_inode = {0};

	for (size_t i = 0; i < MAX_SECTORS_FOR_A_FILE; i++)
	{
		new_inode.data_locations[i] = -1;
	}

	return new_inode;
}


void MyFs::create_file(const std::string& path, bool directory)
{
	uint32_t inode_number;
	if (path != "/")
	{
		inode_number = pre_create_checks(path);
	}
	else
	{
		inode_number = search_free_bit(BITMAP_TABLE_ADDRESS, BITMAP_TABLE_SECTORS_REQUIRED);
	}

	inode new_inode = initialize_inode();
	write_new_inode(inode_number, new_inode);

	if (path != "/")
	{
		std::string file_name = get_file_name_from_path(path);
		DirEntry entry = initialize_entry(file_name, directory, inode_number);

		uint32_t parent_inode_number = get_parent_inode_number(path);
		inode parent_inode = get_inode(parent_inode_number);

		write_entry_to_dir(entry, parent_inode, parent_inode_number);
	}
}


uint32_t MyFs::get_parent_inode_number(const std::string& path)
{
	std::string parent_path = get_parent_path(path);
	if (parent_path == "/") return 0;

	std::string grandfather_path = get_parent_path(parent_path);
	dir_entry_list grandfather_entries = ls_command(grandfather_path);

	std::string parent_name = get_file_name_from_path(parent_path);

	for (size_t i = 0; i < grandfather_entries.size(); i++)
	{
		if (std::strncmp(reinterpret_cast<const char*>(grandfather_entries.at(i).name), parent_name.c_str(), NAME_SIZE) == 0)
		{
			return grandfather_entries.at(i).inode_number;
		}
	}
	throw std::runtime_error("Error: couldn't find parent inode");
}


/*
bool MyFs::does_dir_last_sector_full(const inode& parent_dir)
{
	int last_sector_addr = parent_dir.data_locations[parent_dir.number_of_sectors - 1];

	MyFs::buffer_data_type buffer = get_sector_data(last_sector_addr);

	if (buffer.at(SECTOR_SIZE - sizeof(DirEntry)) != '\0') return true;
	return false;
}
*/


void MyFs::write_entry_to_dir(const DirEntry& entry, inode& dir_inode, const uint32_t& inode_number)
{
	int sector_number = get_sector_number_to_write_entry(dir_inode);

	write_new_inode(inode_number, dir_inode);

	uint32_t target_addr = CONTENT_ADDRESS + (sector_number * SECTOR_SIZE);

	MyFs::buffer_data_type sector_buffer = get_sector_data(target_addr);

	DirEntry* dirEntry_array = reinterpret_cast<DirEntry*>(sector_buffer.data());

	uint16_t offset = calc_offset_for_dirEntry(dirEntry_array);
	
	dirEntry_array[offset] = entry;

	blkdevsim->write(target_addr, reinterpret_cast<char*>(dirEntry_array));
}


uint32_t MyFs::get_sector_number_to_write_entry(inode& dir_inode)
{
    for (int i = 0; i < MAX_SECTORS_FOR_A_FILE; i++)
    {
        if (dir_inode.data_locations[i] == std::numeric_limits<uint32_t>::max())
        {
            uint32_t new_sector = search_free_bit(BITMAP_CONTENT_ADDRESS, BITMAP_CONTENT_SECTORS_REQUIRED);
            dir_inode.data_locations[i] = new_sector;
            return new_sector;
        }

        if (!is_sector_full(dir_inode.data_locations[i], sizeof(DirEntry)))
        {
            return dir_inode.data_locations[i];
        }
    }
	throw std::runtime_error("Error: this directory is already full (" + std::to_string(ENTRIES_PER_SECTOR * MAX_SECTORS_FOR_A_FILE) + " files)");
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


std::array<uint32_t, 2> MyFs::calc_inode_write_locations(uint32_t inode_number)
{
	uint32_t sector_addr = INODE_TABLE_ADDRESS + (inode_number / INODES_PER_SECTOR) * SECTOR_SIZE;
	uint32_t offset = (inode_number % INODES_PER_SECTOR) * sizeof(inode);

	std::array<uint32_t, 2> locations = {}; // location[0] = sector, location[1] = offset

	locations[0] = sector_addr;
	locations[1] = offset;
	return locations;
}


DirEntry MyFs::initialize_entry(const std::string& file_name, uint16_t is_dir, const uint32_t& inode_number)
{
	DirEntry entry = {0};

	std::memcpy(entry.name, file_name.c_str(), file_name.size());	
	entry.inode_number = inode_number;
	entry.is_dir = is_dir;

	return entry;
}


std::string MyFs::get_content(const std::string& path)
{
    std::string content;
    DirEntry file_entry = get_file_entry_from_path(path);
 
	if (file_entry.is_dir) throw std::runtime_error("Error: cat operation is not possible on directory");

	inode file_inode = get_inode(file_entry.inode_number);

    for (size_t i = 0; i < MAX_SECTORS_FOR_A_FILE; i++)
    {
        uint32_t sector_number = file_inode.data_locations[i];
        
        if (sector_number == std::numeric_limits<uint32_t>::max()) break;

        uint32_t phys_addr = CONTENT_ADDRESS + (sector_number * SECTOR_SIZE);
        buffer_data_type sector_data = get_sector_data(phys_addr);

		for (size_t i = 0; i < sector_data.size(); i++)
		{
			if (sector_data.at(i) == '\0') break;
			content += sector_data.at(i);
		}
    }
    return content;
}


std::vector<uint32_t> MyFs::write_to_new_sectors(std::string content, std::vector<uint32_t> written_sectors)
{
	std::string curr_sector_content;

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

		uint32_t free_sector_number = search_free_bit(BITMAP_CONTENT_ADDRESS, BITMAP_CONTENT_SECTORS_REQUIRED);
		written_sectors.push_back(free_sector_number);

		uint32_t addr = CONTENT_ADDRESS + (free_sector_number * SECTOR_SIZE);
		MyFs::buffer_data_type sector_vector = get_sector_data(addr);

		std::copy(curr_sector_content.begin(), curr_sector_content.begin() + curr_sector_content.size(), sector_vector.begin());

		blkdevsim->write(addr, reinterpret_cast<char*>(sector_vector.data()));
	}
	return written_sectors;
}


std::vector<uint32_t> MyFs::append_content_to_file(std::string content, DirEntry& entry, std::vector<uint32_t> written_sectors)
{
	inode file_inode = get_inode(entry.inode_number);
	uint16_t remaining_space_in_last_sector = 0;

	uint32_t last_sector = file_inode.data_locations[(entry.file_size / SECTOR_SIZE)];
	if (last_sector == std::numeric_limits<uint32_t>::max())
	{
		last_sector = search_free_bit(BITMAP_CONTENT_ADDRESS, BITMAP_CONTENT_SECTORS_REQUIRED);
		written_sectors.push_back(last_sector);
		remaining_space_in_last_sector = 512;
	}

	uint32_t last_sector_addr = CONTENT_ADDRESS + (last_sector * SECTOR_SIZE);

	remaining_space_in_last_sector = calc_remain_space_in_last_sector(last_sector_addr);

	if (remaining_space_in_last_sector > 0)
	{
		return append_to_last_sector(content, last_sector_addr, remaining_space_in_last_sector, written_sectors);
	}
	else
	{
		return write_to_new_sectors(content, written_sectors);
	}
}


std::vector<uint32_t> MyFs::append_to_last_sector(std::string& content, uint32_t last_sector_addr, int remaining_space_in_last_sector, std::vector<uint32_t> written_sectors)
{
	MyFs::buffer_data_type sector_data = get_sector_data(last_sector_addr);
	uint32_t offset = SECTOR_SIZE - remaining_space_in_last_sector;

	size_t bytes_to_copy = std::min<size_t>(content.size(), remaining_space_in_last_sector);

	std::copy(content.begin(), content.begin() + bytes_to_copy, sector_data.begin() + offset);
	
	blkdevsim->write(last_sector_addr, reinterpret_cast<char*>(sector_data.data()));

	if (content.size() > static_cast<size_t>(remaining_space_in_last_sector))
	{
        std::string leftovers = content.substr(bytes_to_copy); 
        return write_to_new_sectors(leftovers, written_sectors);
	}
	return written_sectors;
}


MyFs::buffer_data_type MyFs::get_sector_data(uint32_t addr)
{
	MyFs::buffer_data_type sector_data = {0};
	blkdevsim->read(addr, reinterpret_cast<char*>(sector_data.data()));
	return sector_data;
}


uint16_t MyFs::calc_remain_space_in_last_sector(uint32_t last_sector_addr)
{
	MyFs::buffer_data_type data = get_sector_data(last_sector_addr);

	auto it = std::find(data.begin(), data.end(), '\0');

	if (it == data.end()) return -1;

	int end_content_index = std::distance(data.begin(), it);

	return SECTOR_SIZE - end_content_index;
}


std::vector<uint32_t> MyFs::handle_write_content(DirEntry& entry, std::string& content)
{
	std::vector<uint32_t> written_sectors = {};

	if (entry.file_size == 0)
	{
		return write_to_new_sectors(content, written_sectors);
	}
	else
	{
		return append_content_to_file(content, entry, written_sectors);
	}
}


DirEntry MyFs::get_file_entry_from_path(const std::string& path)
{
	std::string parent_path = get_parent_path(path);
	dir_entry_list parent_entries = ls_command(parent_path);

	std::string file_name = get_file_name_from_path(path);

	for (const DirEntry& curr_entry : parent_entries)
	{
		if (std::strncmp(reinterpret_cast<const char*>(curr_entry.name), file_name.c_str(), NAME_SIZE) == 0)
		{
			return curr_entry;
		}
	}
	throw std::runtime_error("Error: there is no file *" + file_name + "* in this directory");
}


void MyFs::update_inode(const std::vector<uint32_t>& written_sectors, inode& file_inode)
{
	uint8_t index = 0;

    for (size_t i = 0; i < MAX_SECTORS_FOR_A_FILE; i++)
    {
		if (index == written_sectors.size()) break;

		if (file_inode.data_locations[i] == std::numeric_limits<uint32_t>::max())
        {
            file_inode.data_locations[i] = written_sectors.at(index);
            index++;
        }
    }
}

void MyFs::check_if_file_reach_max_data(const inode& file_inode)
{
	if (file_inode.data_locations[MAX_SECTORS_FOR_A_FILE - 1] != std::numeric_limits<uint32_t>::max() && is_sector_full(file_inode.data_locations[MAX_SECTORS_FOR_A_FILE - 1], 1))
		throw std::runtime_error("Error: file has reached limit size");
}


void MyFs::set_content(std::string& path, std::string& content)
{
	DirEntry file_entry = get_file_entry_from_path(path);
	if (file_entry.is_dir) throw std::runtime_error("Error: can't edit a folder");

	inode file_inode = get_inode(file_entry.inode_number);

	// check - file reach max data_locations
	check_if_file_reach_max_data(file_inode);

	std::vector<uint32_t> written_sectors = handle_write_content(file_entry, content);

	update_inode(written_sectors, file_inode);

	write_new_inode(file_entry.inode_number, file_inode);

	update_entries_recursive(path, content);
}


void MyFs::update_entries_recursive(std::string& path, const std::string& content)
{
	if (path == "/") return;
	
	std::string parent_path = get_parent_path(path);
	update_entries_recursive(parent_path, content);

	int parent_inode_number = get_parent_inode_number(path);
	inode parent_inode = get_inode(parent_inode_number);

	DirEntry file_entry = get_file_entry_from_path(path);
	file_entry.file_size += content.size();

	update_entry(file_entry, parent_inode);
}


void MyFs::update_entry(const DirEntry& file_entry, const inode& parent_inode)
{
	dir_entry_list entries;
	dir_entry_list curr_sector_entries(ENTRIES_PER_SECTOR);
	uint32_t addr;
	uint8_t entry_number = 0;

	for (int i = 0; i < MAX_SECTORS_FOR_A_FILE; i++)
	{
		if (parent_inode.data_locations[i] != std::numeric_limits<uint32_t>::max())
		{
			addr = CONTENT_ADDRESS + SECTOR_SIZE * parent_inode.data_locations[i];
			blkdevsim->read(addr, reinterpret_cast<char*>(curr_sector_entries.data()));

			for (const auto& curr_entry : curr_sector_entries)
			{
				if(curr_entry.name[0] == 0) break;
				
				else if (std::memcmp(file_entry.name, curr_entry.name, NAME_SIZE) == 0)
				{
					curr_sector_entries[entry_number] = file_entry;
					blkdevsim->write(addr, reinterpret_cast<char*>(curr_sector_entries.data()));
					return;
				}
				entry_number++;
			}
		}
		else break;
		entry_number = 0;
	}
	throw std::runtime_error("Error: path is incorrect");
}


MyFs::dir_entry_list MyFs::get_dir_entries(const uint32_t& inode_number)
{
	inode dir_inode = get_inode(inode_number);

	dir_entry_list entries;
	dir_entry_list curr_sector_entries(ENTRIES_PER_SECTOR);
	uint32_t addr;
	for (int i = 0; i < MAX_SECTORS_FOR_A_FILE; i++)
	{
		if (dir_inode.data_locations[i] != std::numeric_limits<uint32_t>::max())
		{
			addr = CONTENT_ADDRESS + SECTOR_SIZE * dir_inode.data_locations[i];
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


uint32_t MyFs::search_free_bit(const uint32_t& start_addr, const uint16_t& amount_of_sectors)
{
	uint32_t curr_addr = start_addr;
	uint32_t end_addr = start_addr + amount_of_sectors * SECTOR_SIZE;

	char buffer[SECTOR_SIZE] = {0};

	while (curr_addr < end_addr)
	{
		blkdevsim->read(curr_addr, buffer);
		for (size_t i = 0; i < SECTOR_SIZE; i++)
		{
			if (buffer[i] == 255) continue; // all byte(8 sectors) are full.

			for (int bit = 0; bit < 8; bit++)
			{
				if (!((buffer[i] >> bit) & 1))
				{
					buffer[i] |= 1 << bit; 
					blkdevsim->write(curr_addr, buffer);
					
					uint32_t sector_num = (i * 8) + bit;
					return sector_num;
				}
			}
		}
		curr_addr += SECTOR_SIZE;
	}
	throw std::runtime_error("Error: no free sector");
}


bool MyFs::does_entry_exists(const dir_entry_list& parent_entries, const std::string& file_name)
{
	for (const DirEntry& entry : parent_entries)
	{
		if (std::memcmp(file_name.c_str(), entry.name, NAME_SIZE) == 0)
		{
			return true;
		}
	}
	return false;
}


bool MyFs::is_sector_full(int sector_to_check, size_t jump_size)
{
	char buffer[SECTOR_SIZE];
	uint32_t addr = CONTENT_ADDRESS + (sector_to_check * SECTOR_SIZE);

	blkdevsim->read(addr, buffer);

	for (size_t i = 0; i < SECTOR_SIZE; i+=jump_size)
	{
		if (buffer[i] == 0)
		{
			return false;
		}
	}
	return true;
}


inode MyFs::get_inode(const uint32_t& inode_number)
{
	inode_list inodes(INODES_PER_SECTOR);
	uint32_t addr = INODE_TABLE_ADDRESS + (inode_number / INODES_PER_SECTOR) * SECTOR_SIZE;

	blkdevsim->read(addr, reinterpret_cast<char*>(inodes.data()));
	return inodes.at(inode_number % INODES_PER_SECTOR);
}


int MyFs::resolve_path(const std::vector<std::string>& parts)
{
	bool found = false;
	dir_entry_list curr_entries = get_dir_entries(0);

	for (size_t i = 0; i < parts.size(); i++)
	{
		for (size_t j = 0; j < curr_entries.size(); j++)
		{
			if (std::strncmp(reinterpret_cast<const char*>(curr_entries.at(j).name), parts.at(i).c_str(), NAME_SIZE) == 0 && curr_entries.at(j).is_dir)
			{
				if (i == parts.size() - 1) return curr_entries.at(j).inode_number;
				curr_entries = get_dir_entries(curr_entries.at(j).inode_number);
				found = true;
			}
		}
		if(!found) throw std::runtime_error("Error: path is incorrect");
		found = false;
	}
	throw std::runtime_error("Error: path is incorrect");
}


MyFs::dir_entry_list MyFs::ls_command(const std::string& path)
{
	if (path == "/") return get_dir_entries(0);

	std::vector<std::string> parts = split_path_by_slash(path);

	int parent_dir_inode_number = resolve_path(parts);
	
	return get_dir_entries(parent_dir_inode_number);
}