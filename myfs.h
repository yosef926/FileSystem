#ifndef __MYFS_H__
#define __MYFS_H__

#include <memory>
#include <vector>
#include <stdint.h>
#include "blkdev.h"
#include "FsData.h"


class MyFs {
private:
	// Global Constants for the Filesystem
	BlockDeviceSimulator *blkdevsim;

public:
	MyFs(BlockDeviceSimulator *blkdevsim_);

	typedef std::vector<inode> inode_list;
	typedef std::vector<DirEntry> dir_entry_list;
	typedef std::array<uint8_t, SECTOR_SIZE> buffer_data_type;
	/**
	 * format method
	 * This function discards the current content in the blockdevice and
	 * create a fresh new MYFS instance in the blockdevice.
	 */
	void format();

	/**
	 * @param path the file path (e.g. "/newfile")
	 * @param directory boolean indicating whether this is a file or directory
	 */
	void create_file(const std::string& path, bool directory);

	/**
	 * Returns the whole content of the file indicated by path param.
	 * Note: this method assumes path refers to a file and not a
	 * directory.
	 * @param path the file path (e.g. "/somefile")
	 * @return the content of the file
	 */
	std::string get_content(const std::string& path);

	/**
	 * Sets the whole content of the file indicated by path param.
	 * Note: this method assumes path refers to a file and not a
	 * directory.
	 * @param path the file path (e.g. "/somefile")
	 * @param content the file content string
	 */
	void set_content(const std::string& path, std::string& content);

	/**
	 * Returns a list of a files in a directory.
	 * Note: this method assumes path refers to a directory and not a
	 * file.
	 * @param path the file path (e.g. "/somedir")
	 * @return a vector of inode structures, one for each file in
	 *	the directory.
	 */
	//dir_entry_list list_root_inodes(const std::string& path);
	dir_entry_list ls_command(const std::string& path);



	// General
	void fill_file_with_null();
	void insert_fs_headers();
	bool is_sector_full(int sector_to_check, size_t jump_size);
	int resolve_path(const std::vector<std::string>& parts);
	buffer_data_type get_sector_data(uint32_t addr);


	// Technical
	uint32_t pre_create_checks(const std::string& path);
	std::vector<std::string> split_path_by_slash(const std::string& path);
	uint32_t search_free_bit(const uint32_t& start_addr, const uint16_t& amount_of_sectors);
	std::string get_parent_path(const std::string& path);
	std::string get_file_name_from_path(const std::string& path);
	void does_file_exists(const dir_entry_list& parent_entries, const std::string& file_name);
	void does_inode_table_full();


	// Files
	void update_entry(const DirEntry& file_entry, const inode& parent_inode);
	std::vector<uint32_t> handle_write_content(DirEntry& entry, std::string& content);
	std::vector<uint32_t> write_to_new_sectors(std::string content);
	std::vector<uint32_t> append_to_last_sector(std::string& content, uint32_t last_sector_addr, int remaining_space_in_last_sector);
	std::vector<uint32_t> append_content_to_file(std::string content, DirEntry& entry);
	uint16_t calc_remain_space_in_last_sector(uint32_t last_sector_addr);
	bool does_entry_exists(const dir_entry_list& parent_entries, const std::string& file_name);
	DirEntry initialize_entry(const std::string& file_name, uint16_t is_dir, const uint32_t& inode_number);
	DirEntry get_file_entry_from_path(const std::string& path);


	// Inode table
	void write_new_inode(const uint32_t& inode_number, const inode& new_inode);
	inode initialize_inode();
	std::vector<inode> map_sector_to_inodes(const std::vector<uint8_t>& buffer);
	inode get_inode(const uint32_t& inode_number);
	std::array<uint32_t, 2> calc_inode_write_locations(uint32_t inode_number);
	uint32_t get_parent_inode_number(const std::string& parent_path);
	void update_inode(const std::vector<uint32_t>& written_sectors, inode& file_inode);


	// Folders(entries, path, etc...)
	int calc_offset_for_dirEntry(DirEntry* dirEntry_array);
	void write_entry_to_dir(const DirEntry& entry, inode& dir_inode, const uint32_t& inode_number);
	dir_entry_list get_dir_entries(const uint32_t& inode_number);
	uint32_t get_sector_number_to_write_entry(inode& dir_inode);
};

#endif // __MYFS_H__