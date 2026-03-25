#include "myfs.h"
#include <string.h>
#include <iostream>
#include <math.h>
#include <sstream>
#include <stdint.h>
#include <cstring>
#include "file.h"

MyFs::MyFs(BlockDeviceSimulator *blkdevsim_):blkdevsim(blkdevsim_) {
	struct myfs_header header;
	blkdevsim->read(0, sizeof(header), (char *)&header);

	if (std::memcmp(header.magic, MYFS_MAGIC, sizeof(header.magic)) != 0 ||
	    (header.version != CURR_VERSION)) {
		std::cout << "Did not find myfs instance on blkdev" << std::endl;
		std::cout << "Creating..." << std::endl;
		format();
		std::cout << "Finished!" << std::endl;
	}
}


void MyFs::format() {
	//Formatting the fs.
	fillFileWithZeros();

	// put the headers in place
	insertFsHeaders();
}


void MyFs::create_file(const std::string& path_str, bool directory) {
	dir_list dirent = list_dir("/");
	if(is_path_exist(dirent, path_str))
	{
		std::cout << "File already exists\n" << std::endl;
		return;
	}
	
	uint32_t addr;
	File newFile(path_str);

	if (!directory)
	{
		strncpy((char*)newFile._entry.name, path_str.c_str(), NAME_SIZE - 1);
		newFile._entry.name[NAME_SIZE] = 0;

		// Find the next free slot in the inode table and insert there the newFile.
		addr = INODE_TABLE_ADDRESS + (dirent.size()) * sizeof(newFile._entry);
	}
	else
	{
		throw std::runtime_error("not implemented");
	}
	blkdevsim->write(addr, sizeof(newFile._entry), (char *)&newFile);
}


std::string MyFs::get_content(const std::string& path_str) {
	dir_list dirent = list_dir("/");
	int inode_number = find_inode_number(path_str, dirent);
	File file = dirent[inode_number];

	std::string ans = "";
	char sector_buffer[SECTOR_SIZE];

	for (size_t i = 0; i < file._entry.number_of_sectors; i++)
	{
		blkdevsim->read(CONTENT_ADDRESS + (file._entry.data_locations[i] * SECTOR_SIZE), SECTOR_SIZE, sector_buffer);
		ans.append(sector_buffer, SECTOR_SIZE);
	}

	return ans;
}

std::vector<int> MyFs::write_to_free_sectors(const std::string& content, const File* file)
{
	const size_t chunkSize = SECTOR_SIZE;
	std::vector<std::string> chunks;
	std::vector<int> written_sectors;

    for (size_t i = 0; i < content.length(); i += chunkSize) {
        // substr handles the end of the string automatically
        chunks.push_back(content.substr(i, chunkSize));
    }

	for(size_t i = 0;  i < chunks.size(); i++)
	{
		int sectorNumber = find_free_sector();
		uint32_t addr = CONTENT_ADDRESS + (sectorNumber * SECTOR_SIZE);
		blkdevsim->write(addr , chunks[i].length(), (char *)chunks[i].c_str());
		written_sectors.push_back(sectorNumber);
	}

	return written_sectors;
}

void MyFs::write_content(int inode_number, File& file, std::string& content)
{
	int total_size = (content.size() - 1) + file._entry.file_size;
	std::vector<int> written_sectors;

	int final_sector = file._entry.number_of_sectors == 0 ? -1 : file._entry.data_locations[file._entry.number_of_sectors-1];

	// Delete last '\n'

	// write all content to new sectors
	if (final_sector == -1 || is_sector_full(final_sector))
	{
		written_sectors = write_to_free_sectors(content, &file);
	}
	else
	{
		char sector_content[SECTOR_SIZE] = {0};

		blkdevsim->read(CONTENT_ADDRESS + (final_sector * SECTOR_SIZE), SECTOR_SIZE, (char*)sector_content);

		std::string str_sector_content(sector_content);
		uint32_t addr = CONTENT_ADDRESS + (final_sector * SECTOR_SIZE) + str_sector_content.length();
		
		// fit into the last sector
		if(str_sector_content.length() + content.length() < SECTOR_SIZE)
		{
			blkdevsim->write(addr, content.length(), (char *)content.c_str());
		}
		else //new content will spread into >= 2 sectors.
		{
			str_sector_content += content.substr(0, SECTOR_SIZE-str_sector_content.length());
			blkdevsim->write(addr , str_sector_content.length(), (char *)str_sector_content.c_str());

			// write left over of content to new sectors
			written_sectors = write_to_free_sectors(content.substr(SECTOR_SIZE-str_sector_content.length(), content.length()), &file);
		}
	}
	update_file_headers(total_size, inode_number, written_sectors, &file);
}


void MyFs::set_content(const std::string& path_str, std::string& content) {
	dir_list dirent = list_dir("/");
	if (!is_path_exist(dirent, path_str))
	{
		std::cout << "\"" << path_str << "\": File doesn't exist" << std::endl;
		return;
	}

	File file = getFileByName(dirent, path_str);
	int inode_number = 0; // fix --> find real inode_number
	write_content(inode_number, file, content);
}


File MyFs::getFileByName(const dir_list& dirent, const std::string& path_str)
{
	int inode_number = find_inode_number(path_str, dirent);
	return dirent[inode_number];
}


MyFs::dir_list MyFs::list_dir(const std::string& path_str) {
	dir_list ans;
	int total_bytes = SECTOR_SIZE * TABLE_SECTORS_AMOUNT;
	ans.resize(total_bytes);
	
	blkdevsim->read(INODE_TABLE_ADDRESS, total_bytes, (char*)ans.data());
	
	dir_list result;
	for (size_t i = 0; i < ans.size(); i++)
	{
		if (ans[i]._entry.name[0] != 0)
		{
			result.push_back(ans[i]);
		}
	}
	return result;
}


void MyFs::fillFileWithZeros()
{
	std::vector<char> buffer(blkdevsim->DEVICE_SIZE, 0);

	blkdevsim->write(0, buffer.size(), buffer.data());
}


void MyFs::insertFsHeaders()
{
	struct myfs_header header;
	
	strncpy((char*)header.magic, (char*)MYFS_MAGIC, sizeof(header.magic));
	header.version = CURR_VERSION;
	header.sector_size = SECTOR_SIZE;
	header.bitMap_address = BIT_MAP_ADDRESS;
	header.inode_table_address = INODE_TABLE_ADDRESS;
	header.content_address = CONTENT_ADDRESS;
	
	blkdevsim->write(0, sizeof(header), (const char*)&header);
}


int MyFs::find_inode_number(const std::string& file_name, const dir_list& dirent)
{
	int inode_number = 0;

	for (size_t i = 0; i < dirent.size(); i++)
	{
		if (!std::strncmp((char*)dirent[i]._entry.name, file_name.c_str(), NAME_SIZE))
		{
			inode_number = i;
		}
	}
	return inode_number;
}


int MyFs::find_free_sector()
{
	uint16_t total_bytes = SECTORES_OF_DATA / 8; 

	std::vector<uint8_t> free_block_bitmap(total_bytes); //exactly (SECTORES_OF_DATA) bits.
	/** free_block_bitmap is x bits, the read function
	 *  should get size of also x bits, which is x / 8 bytes(memcpy read by bytes and not bits).
	 */
	blkdevsim->read(BIT_MAP_ADDRESS, total_bytes, (char*)free_block_bitmap.data());

	for (size_t i = 0; i < total_bytes; i++)
	{
		if (free_block_bitmap[i] == 255) continue; // all byte(8 sectors) are full.

		for (int bit = 0; bit < 8; bit++)
		{
			if (!((free_block_bitmap[i] >> bit) & 1)) {
				free_block_bitmap[i] |= 1 << bit; 
				blkdevsim->write(BIT_MAP_ADDRESS + i, 1, (const char*)&free_block_bitmap[i]);
				
				int sector_num = (i * 8) + bit;
				return sector_num;
			}
		}
	}
	return -1;
}


bool MyFs::is_path_exist(const dir_list& dirent, const std::string& file_name)
{
	for (size_t i = 0; i < dirent.size(); i++)
	{
		if (!std::strncmp((char*)dirent[i]._entry.name, file_name.c_str(), NAME_SIZE)) return true;
	}
	return false;
}


void MyFs::update_file_headers(int total_size, int inode_number, const std::vector<int>& sectors, File* file) 
{
	file->_entry.file_size = total_size;
	
	size_t start_idx = file->_entry.number_of_sectors;
	for(size_t i = 0; i < sectors.size(); i++) 
	{
		file->_entry.data_locations[start_idx + i] = sectors[i];
	}

	file->_entry.number_of_sectors += sectors.size();
	int write_offset = INODE_TABLE_ADDRESS + (inode_number * sizeof(file->_entry));
	blkdevsim->write(write_offset, sizeof(file->_entry), (char *)&(file->_entry));
}


bool MyFs::is_sector_full(int sector_to_check)
{
	uint8_t buffer[SECTOR_SIZE];
	blkdevsim->read(CONTENT_ADDRESS + (sector_to_check * SECTOR_SIZE), SECTOR_SIZE, (char*)buffer);

	for (size_t i = 0; i < SECTOR_SIZE; i++)
	{
		if (buffer[i] == 0)
		{
			return false;
		}
	}
	return true;
}