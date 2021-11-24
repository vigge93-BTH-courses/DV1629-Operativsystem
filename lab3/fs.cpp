#include <iostream>
#include <cstring>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    disk.read(FAT_BLOCK, (uint8_t*)&fat);
    disk.read(ROOT_BLOCK, (uint8_t*)&root_dir);
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
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        memset(root_dir[i].file_name, 0, 56);
        root_dir[i].size = 0;
        root_dir[i].first_blk = 0;
        root_dir[i].type = 0;
        root_dir[i].access_rights = 0;
    }
    std::cout << BLOCK_SIZE/sizeof(dir_entry) << " entries in root\n";
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    disk.write(ROOT_BLOCK, (uint8_t*)root_dir);
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
    
    // dir_entry dir[BLOCK_SIZE/sizeof(dir_entry)];
    // int read = disk.read(ROOT_BLOCK, (uint8_t*)dir);
    // if (read == -1) {
    //     std::cout << "Error reading directory\n";
    //     return -1;
    // }
    
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (root_dir[i].first_blk == 0) {
            root_dir[i] = file;
            break;
        }
    }
    disk.write(ROOT_BLOCK, (uint8_t*)root_dir);
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
    std::string dirpath = filepath.substr(0, filepath.find_last_of("/"));
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(root_dir[i].file_name, filename.c_str(), 56) == 0) {
            int blk_no = root_dir[i].first_blk;
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
    std::cout << "name\tsize\n";
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (root_dir[i].first_blk != 0) {
            std::cout << root_dir[i].file_name << "\t" << root_dir[i].size << "\n";
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

    std::string source_filename = sourcepath.substr(sourcepath.find_last_of("/") + 1);
    std::string dest_filename = destpath.substr(destpath.find_last_of("/") + 1);
    std::string dirpath = sourcepath.substr(0, sourcepath.find_last_of("/"));

    dir_entry new_file;
    strncpy(new_file.file_name, dest_filename.c_str(), 56);
    new_file.first_blk = 0;

    int current_blk = -1;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(root_dir[i].file_name, source_filename.c_str(), 56) == 0) {
            new_file.size = root_dir[i].size;
            new_file.type = root_dir[i].type;
            new_file.access_rights = root_dir[i].access_rights;
            current_blk = root_dir[i].first_blk;
            break;
        }
    }
    if (current_blk == -1) {
        std::cout << "File not found\n";
        return -1;
    }

    int current_new_blk = -1;
    while (current_blk != FAT_EOF) {
        uint8_t buf[BLOCK_SIZE];
        disk.read(current_blk, buf);
        int new_blk_no = -1;
        for(int i = 0; i < BLOCK_SIZE/2; i++) {
            if (fat[i] == FAT_FREE) {
                new_blk_no = i;
                break;
            }
        }
        if (new_blk_no == -1) {
            std::cout << "No free blocks\n";
            return -1;
        }
        if (new_file.first_blk == 0) {
            new_file.first_blk = new_blk_no;
        } else {
            fat[current_new_blk] = new_blk_no;
        }
        current_new_blk = new_blk_no;
        current_blk = fat[current_blk];
        disk.write(new_blk_no, buf);
    }
    fat[current_new_blk] = FAT_EOF;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (root_dir[i].first_blk == 0) {
            root_dir[i] = new_file;
            break;
        }
    }
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    disk.write(ROOT_BLOCK, (uint8_t*)root_dir);
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    
    std::string filename = sourcepath.substr(sourcepath.find_last_of("/") + 1);
    std::string dirpath = sourcepath.substr(0, sourcepath.find_last_of("/"));
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(root_dir[i].file_name, filename.c_str(), 56) == 0) {
            strncpy(root_dir[i].file_name, destpath.substr(destpath.find_last_of("/") + 1).c_str(), 56);
            break;
        }
    }
    disk.write(ROOT_BLOCK, (uint8_t*)root_dir);
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";

    std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
    std::string dirpath = filepath.substr(0, filepath.find_last_of("/"));
    int block_no = 0;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(root_dir[i].file_name, filename.c_str(), 56) == 0) {
            block_no = root_dir[i].first_blk;
            root_dir[i].first_blk = 0;
            root_dir[i].size = 0;
            memset(root_dir[i].file_name, 0, 56);
            break;
        }
    }
    while (block_no != FAT_EOF) {
        int next_blk = fat[block_no];
        fat[block_no] = FAT_FREE;
        block_no = next_blk;
    }
    disk.write(ROOT_BLOCK, (uint8_t*)root_dir);
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    std::string filename1 = filepath1.substr(filepath1.find_last_of("/") + 1);
    std::string filename2 = filepath2.substr(filepath2.find_last_of("/") + 1);

    dir_entry file1;
    dir_entry *file2;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(root_dir[i].file_name, filename1.c_str(), 56) == 0) {
            file1 = root_dir[i];
        }
        if (strncmp(root_dir[i].file_name, filename2.c_str(), 56) == 0) {
            file2 = &root_dir[i];
        }
    }
    if (file1.first_blk == 0 || file2->first_blk == 0) {
        std::cout << "File not found\n";
        return -1;
    }
    int last_blk = file2->first_blk;
    while (fat[last_blk] != FAT_EOF) {
        last_blk = fat[last_blk];
    }

    int last_blk_size = file1.size % BLOCK_SIZE;
    file2->size = file1.size + file2->size;
    std::string data;
    uint8_t buf[BLOCK_SIZE];
    disk.read(last_blk, buf);
    data.append((char*)buf, last_blk_size);
    int current_blk = file1.first_blk;
    int size_left = file1.size;
    while (current_blk != FAT_EOF && size_left > 0) {
        disk.read(current_blk, buf);
        data.append((char*)buf, std::min(size_left, BLOCK_SIZE));
        current_blk = fat[current_blk];
        size_left -= BLOCK_SIZE;
    }

    disk.write(last_blk, (uint8_t*)data.c_str());
    std::cout << data.length() << std::endl;
    if (data.length() > BLOCK_SIZE)
        data = data.substr(BLOCK_SIZE);
    else
        data = "";

    while(data.length() > 0) {
        int prev_blk_no = last_blk;
        int last_blk = -1;
        for(int i = 0; i < BLOCK_SIZE/2; i++) {
            if (fat[i] == FAT_FREE) {
                last_blk = i;
                break;
            }
        }
        if (last_blk == -1) {
            std::cout << "No free blocks\n";
            return -1;
        }
        fat[prev_blk_no] = last_blk;
        disk.write(last_blk, (uint8_t*)data.c_str());
        if (data.length() > 0) {
            data = data.substr(BLOCK_SIZE);
        } else {
            data = "";
        }
    }
    fat[last_blk] = FAT_EOF;
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    disk.write(ROOT_BLOCK, (uint8_t*)root_dir);
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
