#ifndef __BLKDEVSIM__H__
#define __BLKDEVSIM__H__

#include <string>

class BlockDeviceSimulator {
public:
	BlockDeviceSimulator(std::string fname);
	~BlockDeviceSimulator();

	void read(int addr, char *ans);
	void write(int addr, const char *data);

	/* !!! MAX 1GB DEVICE_SIZE !!! */
	static const int DEVICE_SIZE = 1024 * 1024;
	
	static const int SECTOR_SIZE = 512;

private:
	int fd;
	unsigned char *filemap;
};

#endif // __BLKDEVSIM__H__
