#include <iostream>
#include <cstring>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    disk.read(FAT_BLOCK, (uint8_t*)&fat);
}

FS::~FS()
{

}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n";
    for (int i = 0; i < BLOCK_SIZE/2; i++) {
        fat[i] = FAT_FREE;
    }
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;
    dir_entry root[BLOCK_SIZE/sizeof(dir_entry)];
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        memset(root[i].file_name, 0, 56);
        root[i].size = 0;
        root[i].first_blk = 0;
        root[i].type = 0;
        root[i].access_rights = 0;
    }
    std::cout << BLOCK_SIZE/sizeof(dir_entry) << " entries in root\n";
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    disk.write(ROOT_BLOCK, (uint8_t*)root);
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    dir_entry file;
    std::cout << "FS::create(" << filepath << ")\n";
    std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
    std::string dirpath = filepath.substr(0, filepath.find_last_of("/"));
    std::string data;
    std::string in;
    while (std::getline(std::cin, in)) {
        if (in.empty())
            break;
        data.append(in);
    }
    int size = data.length();
    strncpy(file.file_name, filename.c_str(), 56);
    file.size = size;
    file.type = 0; // File
    file.access_rights = 0x04 | 0x02; // read & write

    // Find empty block
    int blk_no = -1;
    for(int i = 0; i < BLOCK_SIZE/2; i++) {
        if (fat[i] == FAT_FREE) {
            blk_no = i;
            break;
        }
    }
    if (blk_no == -1) {
        std::cout << "No free blocks\n";
        return -1;
    }
    file.first_blk = blk_no;
    disk.write(blk_no, (uint8_t*)data.c_str());
    if (data.length() > BLOCK_SIZE)
        data = data.substr(BLOCK_SIZE);
    else
        data = "";

    while(data.length() > 0) {
        int prev_blk_no = blk_no;
        int blk_no = -1;
        for(int i = 0; i < BLOCK_SIZE/2; i++) {
            if (fat[i] == FAT_FREE) {
                blk_no = i;
                break;
            }
        }
        if (blk_no == -1) {
            std::cout << "No free blocks\n";
            return -1;
        }
        fat[prev_blk_no] = blk_no;
        disk.write(blk_no, (uint8_t*)data.c_str());
        if (data.length() > 0) {
            data = data.substr(BLOCK_SIZE);
        } else {
            data = "";
        }
    }
    fat[blk_no] = FAT_EOF;
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    
    dir_entry dir[BLOCK_SIZE/sizeof(dir_entry)];
    int read = disk.read(ROOT_BLOCK, (uint8_t*)dir);
    if (read == -1) {
        std::cout << "Error reading directory\n";
        return -1;
    }
    
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        std::cout << i  << ": " << dir[i].first_blk << "\n";
        if (dir[i].first_blk == 0) {
            dir[i] = file;
            break;
        }
    }
    disk.write(ROOT_BLOCK, (uint8_t*)dir);
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    dir_entry dir[BLOCK_SIZE/sizeof(dir_entry)];
    int read = disk.read(ROOT_BLOCK, (uint8_t*)dir);
    if (read == -1) {
        std::cout << "Error reading directory\n";
        return -1;
    }
    std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
    std::string dirpath = filepath.substr(0, filepath.find_last_of("/"));
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(dir[i].file_name, filename.c_str(), 56) == 0) {
            int blk_no = dir[i].first_blk;
            while (blk_no != FAT_EOF) {
                uint8_t buf[BLOCK_SIZE];
                disk.read(blk_no, buf);
                std::cout << buf;
                blk_no = fat[blk_no];
            }
            std::cout << std::endl;
            break;
        }
    }
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";
    dir_entry dir[BLOCK_SIZE/sizeof(dir_entry)];
    int read = disk.read(ROOT_BLOCK, (uint8_t*)dir);
    if (read == -1) {
        std::cout << "Error reading directory\n";
        return -1;
    }
    std::cout << "name\tsize\n";
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (dir[i].first_blk != 0) {
            std::cout << dir[i].file_name << "\t" << dir[i].size << "\n";
        }
    }
    std::cout << std::endl;
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
