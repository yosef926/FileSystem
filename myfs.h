#ifndef __MYFS_H__
#define __MYFS_H__

#include <memory>
#include <vector>
#include <stdint.h>
#include "blkdev.h"
#include "file.h"

//I put 46, becuase total sectors in fs is 2048, so 46+2 sectors(bitmap and headers) is exactly 48, which leave exactly 2000 sectors ot the data part.
#define TABLE_SECTORS_AMOUNT 46
#define SECTORES_OF_DATA 2000

class MyFs {
private:
	// Global Constants for the Filesystem
	BlockDeviceSimulator *blkdevsim;
	static constexpr uint16_t CURR_VERSION = 3;
	static constexpr uint16_t SECTOR_SIZE = 512;
	static constexpr uint8_t INODES_PER_SECTOR = SECTOR_SIZE / sizeof(inode);
	static constexpr uint16_t PADDING_BYTES_TABLE = SECTOR_SIZE % (INODES_PER_SECTOR * sizeof(inode)); // Not using right now
	static constexpr uint16_t MAX_FILES = TABLE_SECTORS_AMOUNT * INODES_PER_SECTOR;

	// Calculated Addresses
	static constexpr uint32_t BIT_MAP_ADDRESS = SECTOR_SIZE;
	static constexpr uint32_t INODE_TABLE_ADDRESS = SECTOR_SIZE * 2;
	static constexpr uint32_t CONTENT_ADDRESS = INODE_TABLE_ADDRESS + TABLE_SECTORS_AMOUNT * SECTOR_SIZE;

	static constexpr uint8_t MYFS_MAGIC[4] = { 'M', 'Y', 'F', 'S' };	

	struct myfs_header {
		uint8_t magic[sizeof(MYFS_MAGIC)];
		uint16_t version;
		uint16_t sector_size;
		uint16_t bitmap_address;
		uint16_t inode_table_address;
		uint16_t content_address;
	};

public:
	MyFs(BlockDeviceSimulator *blkdevsim_);

	typedef std::vector<File> dir_list;
	using buffer_data_type = std::array<uint8_t, SECTOR_SIZE>;
	/**
	 * format method
	 * This function discards the current content in the blockdevice and
	 * create a fresh new MYFS instance in the blockdevice.
	 */
	void format();

	/**
	 * create_file method
	 * Creates a new file in the required path.
	 * @param path_str the file path (e.g. "/newfile")
	 * @param directory boolean indicating whether this is a file or directory
	 */
	void create_file(const std::string& path_str, bool directory);

	/**
	 * get_content method
	 * Returns the whole content of the file indicated by path_str param.
	 * Note: this method assumes path_str refers to a file and not a
	 * directory.
	 * @param path_str the file path (e.g. "/somefile")
	 * @return the content of the file
	 */
	std::string get_content(const std::string& path_str);

	/**
	 * set_content method
	 * Sets the whole content of the file indicated by path_str param.
	 * Note: this method assumes path_str refers to a file and not a
	 * directory.
	 * @param path_str the file path (e.g. "/somefile")
	 * @param content the file content string
	 */
	void set_content(const std::string& path_str, std::string& content);

	/**
	 * list_dir method
	 * Returns a list of a files in a directory.
	 * Note: this method assumes path_str refers to a directory and not a
	 * file.
	 * @param path_str the file path (e.g. "/somedir")
	 * @return a vector of file_entry structures, one for each file in
	 *	the directory.
	 */
	dir_list list_dir(const std::string& path_str);

	buffer_data_type get_sector_data(uint32_t addr);

	void fill_file_with_null();
	void insert_fs_headers();
	int find_free_sector();
	bool is_path_exist(const dir_list& dirent, const std::string& file_name);
	int find_inode_number(const std::string& file_name, const dir_list& dirent);
	bool is_sector_full(int sector_to_check);

	void update_file_headers(const std::string& content, const std::vector<int>& sectors, File& file);	
	void update_inode_table(const File& file);

	void handle_write_content(File& file, std::string& content);
	std::vector<int> append_to_last_sector(std::string& content, File& file, uint32_t last_sector_addr, int remaining_space_in_last_sector);
	std::vector<int> write_to_new_sectors(std::string& content);
	std::vector<int> append_content_to_file(std::string& content, File& file);
	int calc_remain_space_in_last_sector(const File& file, uint32_t last_sector_addr);
	File find_file(const std::string& path_str, const dir_list& dirent);
	File initialize_file(const std::string& path_str, uint16_t inode_number);
	std::array<uint16_t, 2> find_available_inode_sector(uint16_t inode_number);
	std::vector<inode> map_sector_to_inodes(const std::vector<uint8_t>& buffer);
};

#endif // __MYFS_H__