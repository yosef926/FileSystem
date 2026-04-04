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
}


void MyFs::insert_fs_headers()
{
	char buffer[SECTOR_SIZE] = {0};

	myfs_header* header_ptr = reinterpret_cast<myfs_header*>(buffer);

	header_ptr->version = CURR_VERSION;
	header_ptr->sector_size = SECTOR_SIZE;
	header_ptr->bitmap_table_address = BITMAP_TABLE_ADDRESS;
	header_ptr->bitmap_content_address = BITMAP_CONTENT_ADDRESS;
	header_ptr->inode_table_address = INODE_TABLE_ADDRESS;
	header_ptr->content_address = CONTENT_ADDRESS;

	std::memcpy(header_ptr->magic, MYFS_MAGIC, sizeof(MYFS_MAGIC));
	
	blkdevsim->write(0, buffer);
}


void MyFs::write_new_inode(const uint32_t& inode_number, const inode& new_inode)
{
	std::array<uint16_t, 2> locations = calc_inode_write_locations(inode_number); // location[0] = sector, location[1] = offset
	
	char buffer[SECTOR_SIZE] = {0};

	blkdevsim->read(locations[0], buffer);

	std::memcpy(buffer + locations[1], &new_inode, sizeof(inode));

	blkdevsim->write(locations[0], buffer);
	std::cout << "sector: " << locations[0] << ", offset: " << locations[1] << std::endl;
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
			throw std::runtime_error("Failed to create file: A file with this name already exists in this folder\n");
		}
	}
}


uint32_t MyFs::pre_create_checks(const std::string& path)
{
	std::string parent_path = get_parent_path(path);

	// Check 1 - if ls_command not crash means all files in path are really folders(except last one which is file OR folder)
	dir_entry_list parent_entries = ls_command(parent_path);

	// Check 2 - if this method throw error means no free sector is available for a new file
	uint32_t inode_number = search_free_bit(BITMAP_TABLE_ADDRESS);

	//check 3
	std::string file_name = get_file_name_from_path(path);
	if (file_name.size() >= NAME_SIZE)
	{
    	throw std::runtime_error("Error: Filename too long!");
	}

	//check 4
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
		inode_number = search_free_bit(BITMAP_TABLE_ADDRESS);
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

	uint16_t target_addr = CONTENT_ADDRESS + (sector_number * SECTOR_SIZE);

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
            uint32_t new_sector = search_free_bit(BITMAP_CONTENT_ADDRESS);
            dir_inode.data_locations[i] = new_sector;
            return new_sector;
        }

        if (!is_sector_full(dir_inode.data_locations[i]))
        {
            return dir_inode.data_locations[i];
        }
    }
    throw std::runtime_error("Error: directory is full (max sectors reached)");
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


std::array<uint16_t, 2> MyFs::calc_inode_write_locations(uint16_t inode_number)
{
	uint16_t sector_addr = INODE_TABLE_ADDRESS + (inode_number / INODES_PER_SECTOR) * SECTOR_SIZE;
	uint16_t offset = (inode_number % INODES_PER_SECTOR) * sizeof(inode);

	std::array<uint16_t, 2> locations = {}; // location[0] = sector, location[1] = offset

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


std::string MyFs::get_content(const std::string& path) {
	/*
	inode_list dirent = list_root_entries();
	inode file = find_inode(path, dirent);

	std::string ans = "";
	char sector_buffer[SECTOR_SIZE];

	for (size_t i = 0; i < file.number_of_sectors; i++)
	{
		uint32_t addr = CONTENT_ADDRESS + (file.data_locations[i] * SECTOR_SIZE);
		blkdevsim->read(addr, sector_buffer);
		ans.append(sector_buffer, SECTOR_SIZE);
	}

	return ans;
	*/
	std::string x = "yosef";
	return x;
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

		int free_sector_number = search_free_bit(BITMAP_CONTENT_ADDRESS);
		written_sectors.push_back(free_sector_number);

		uint32_t addr = CONTENT_ADDRESS + (free_sector_number * SECTOR_SIZE);
		MyFs::buffer_data_type sector_vector = get_sector_data(addr);

		std::copy(curr_sector_content.begin(), curr_sector_content.begin() + curr_sector_content.size(), sector_vector.begin());

		blkdevsim->write(addr, reinterpret_cast<char*>(sector_vector.data()));
	}
	return written_sectors;
}


std::vector<int> MyFs::append_content_to_file(std::string content, DirEntry& entry)
{
	inode file_inode = get_inode(entry.inode_number);

	int last_sector = file_inode.data_locations[(entry.file_size / SECTOR_SIZE) - 1];
	uint32_t last_sector_addr = CONTENT_ADDRESS + (last_sector * SECTOR_SIZE);

	int remaining_space_in_last_sector = calc_remain_space_in_last_sector(last_sector_addr);

	if (remaining_space_in_last_sector > 0)
	{
		return append_to_last_sector(content, last_sector_addr, remaining_space_in_last_sector);
	}
	else
	{
		return write_to_new_sectors(content);
	}
}


std::vector<int> MyFs::append_to_last_sector(std::string& content, uint32_t last_sector_addr, int remaining_space_in_last_sector)
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


int MyFs::calc_remain_space_in_last_sector(uint32_t last_sector_addr)
{
	MyFs::buffer_data_type data = get_sector_data(last_sector_addr);

	auto it = std::find(data.begin(), data.end(), '\0');

	if (it == data.end()) return -1;

	int end_content_index = std::distance(data.begin(), it);

	return SECTOR_SIZE - end_content_index;
}


void MyFs::handle_write_content(DirEntry& entry, std::string& content)
{
	/*
	std::vector<int> written_sectors;

	if (entry.file_size == 0)
	{
		written_sectors = write_to_new_sectors(content);
	}
	else
	{
		written_sectors = append_content_to_file(content, entry);
	}
	update_file_headers(content, written_sectors, entry);
	*/
}


void MyFs::set_content(const std::string& path, std::string& content) {
	/*
	inode_list dirent = list_root_inodes("/");

	if (!is_path_exist(dirent, path))
	{
		throw std::runtime_error("inode not found: " + path);
	}

	content.push_back('\0');

	inode file = get_inode(inode_number);
	handle_write_content(file, content);
	*/
	return;
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
				//std::cout << dir_entry.inode_number << std::endl;
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


int MyFs::find_inode_number(const std::string& file_name, const dir_entry_list& dirent)
{
	for (const DirEntry& entry : dirent)
	{
		if (std::memcmp(file_name.c_str(), entry.name, NAME_SIZE) == 0)
		{
			return entry.inode_number;
		}
	}
	throw std::runtime_error("Error: file not found");
}


int MyFs::search_free_bit(const uint16_t& addr)
{
	char buffer[SECTOR_SIZE] = {0};

	blkdevsim->read(addr, buffer);

	for (size_t i = 0; i < SECTOR_SIZE; i++)
	{
		if (buffer[i] == 255) continue; // all byte(8 sectors) are full.

		for (int bit = 0; bit < 8; bit++)
		{
			if (!((buffer[i] >> bit) & 1))
			{
				buffer[i] |= 1 << bit; 
				blkdevsim->write(addr, buffer);
				
				int sector_num = (i * 8) + bit;
				return sector_num;
			}
		}
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


void MyFs::update_entry(const std::string& content, const std::vector<int>& sectors, DirEntry& entry) 
{
	/*
	entry.file_size += (content.size());
;
	size_t start_idx = entry.number_of_sectors;
	for(size_t i = 0; i < sectors.size(); i++) 
	{
		entry.data_locations[start_idx + i] = sectors[i];
	}

	file.number_of_sectors += sectors.size();
	write_new_file_metadata(file);
	*/
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
		std::cout << parts.at(i) << std::endl;
		for (size_t j = 0; j < curr_entries.size(); j++)
		{
			if (std::memcmp(parts.at(i).c_str(), curr_entries.at(i).name, NAME_SIZE) == 0 && curr_entries.at(i).is_dir)
			{
				//std::cout << curr_entries.at(j).inode_number << std::endl;
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