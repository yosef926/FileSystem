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
	 * list_dir_entries method
	 * Returns a list of a files in a directory.
	 * Note: this method assumes path_str refers to a directory and not a
	 * file.
	 * @param path_str the file path (e.g. "/somedir")
	 * @return a vector of inode structures, one for each file in
	 *	the directory.
	 */
	//dir_entry_list list_root_inodes(const std::string& path_str);
	dir_entry_list list_root_entries();
	dir_entry_list list_dir_entries(const std::string& path_str);

	dir_entry_list get_entries_under_dir(const inode& inode);
	inode MyFs::get_inode(uint32_t& inode_number)
	buffer_data_type get_sector_data(uint32_t addr);

	void fill_file_with_null();
	void insert_fs_headers();
	bool is_path_exist(const inode_list& dirent, const std::string& file_name);
	bool is_sector_full(int sector_to_check);

	void update_file_headers(const std::string& content, const std::vector<int>& sectors, inode& file);	


	std::vector<int> append_to_last_sector(std::string& content, inode& file, uint32_t last_sector_addr, int remaining_space_in_last_sector);
	std::vector<int> append_content_to_file(std::string content, inode& file);
	int calc_remain_space_in_last_sector(const inode& file, uint32_t last_sector_addr);

	inode find_inode(const std::string& path_str, const inode_list& dirent);
	int find_inode_number(const std::string& file_name, const inode_list& dirent);
	std::array<uint16_t, 2> find_available_inode_sector(uint16_t inode_number);
	int find_free_sector();

	void handle_write_content(inode& file, std::string& content);
	void write_file_to_disk(const inode& file, const inode_list& dirent);
	void write_entry_to_dir(const DirEntry& entry, int sector_number);
	void write_new_file_metadata(const inode& file);
	std::vector<int> write_to_new_sectors(std::string content);

	inode initialize_inode(const std::string& path_str, uint16_t inode_number, uint8_t is_dir);
	std::vector<inode> map_sector_to_inodes(const std::vector<uint8_t>& buffer);
	bool technical_tests(const inode_list& dirent, const std::string& path_str);
	void update_inode_table(const inode& partent_inode);

	std::vector<std::string> split_path_by_slash(const std::string& path_str);

	// Folders - related
	void handle_adding_entry_to_dir(const inode_list& dirent, const inode& file);
	DirEntry create_dir_entry(const std::string& file_name, const uint32_t& inode_number);
	int update_parent_inode_metadata(const DirEntry& entry, inode& parent_inode);
	bool does_dir_last_sector_full(const inode& parent_dir);
	int calc_offset_for_dirEntry(DirEntry* dirEntry_array);
	int resolve_path(const std::vector<std::string>& parts);
	std::string extract_parent_dir_name(const std::string& path_str);
};

#endif // __MYFS_H__