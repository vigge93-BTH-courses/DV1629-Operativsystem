#include <iostream>
#include <cstring>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    disk.read(FAT_BLOCK, (uint8_t*)fat);
    disk.read(ROOT_BLOCK, (uint8_t*)root_dir);
}

FS::~FS()
{
    disk.write(FAT_BLOCK, (uint8_t*)&fat);
    disk.write(ROOT_BLOCK, (uint8_t*)&root_dir);
}

void
FS::get_filename_parts(std::string filepath, std::string *filename, std::string *dirpath) {
    *filename = filepath.substr(filepath.find_last_of("/") + 1);
    *dirpath = filepath.substr(0, filepath.find_last_of("/"));
}

int
FS::find_empty_block() { 
    int blk_no = -1;
    for(int i = 0; i < BLOCK_SIZE/2; i++) {
        if (fat[i] == FAT_FREE) {
            blk_no = i;
            break;
        }
    }
    if (blk_no == -1) {
        return -1;
    }
    return blk_no;
}

int
FS::write_data(int starting_block, std::string data) {
    int blk_no = starting_block;
    while(data.length() > 0) {
        disk.write(blk_no, (uint8_t*)data.c_str());
        if (data.length() > BLOCK_SIZE) {
            data = data.substr(BLOCK_SIZE);
        } else {
            data = "";
            break;
        }
        int prev_blk_no = blk_no;
        int blk_no = find_empty_block();
        if (blk_no == -1) {
            return -1;
        }
        fat[prev_blk_no] = blk_no;
    }
    fat[blk_no] = FAT_EOF;
    return 0;
}

int
FS::read_data(int start_blk, uint8_t* out_buf, int size) { 
    int current_blk =  start_blk;
    int i = 0;
    uint8_t buf[BLOCK_SIZE];
    while (current_blk != FAT_EOF && i < size) {
        disk.read(current_blk, buf);
        memcpy(&out_buf[i*BLOCK_SIZE], buf, std::min(BLOCK_SIZE, size - i*BLOCK_SIZE));
        current_blk = fat[current_blk];
        ++i;
    }
    return 0;
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
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
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    dir_entry file;
    std::string filename;
    std::string dirpath;
    get_filename_parts(filepath, &filename, &dirpath);

    std::string data;
    std::string in;
    while (std::getline(std::cin, in)) {
        if (in.empty())
            break;
        data.append(in);
    }

    int size = data.length() + 1; // Include null terminator.
    strncpy(file.file_name, filename.c_str(), 56);
    file.size = size;
    file.type = 0; // File
    file.access_rights = 0x04 | 0x02; // read & write

    int blk_no = find_empty_block();
    if (blk_no == -1) {
        std::cout << "No free blocks\n";
        return -1;
    }
    file.first_blk = blk_no;

    int dir_index = -1;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (root_dir[i].first_blk == 0) {
            root_dir[i] = file;
            dir_index = i;
            break;
        }
    }
    if (dir_index == -1) {
        std::cout << "No free space in directory\n";
        return -1;
    }

    int success = write_data(file.first_blk, data);
    if (success == -1) {
        std::cout << "No free blocks\n";
        return -1;
    }
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::string filename;
    std::string dirpath;
    get_filename_parts(filepath, &filename, &dirpath);
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(root_dir[i].file_name, filename.c_str(), 56) == 0) {
            int blk_no = root_dir[i].first_blk;
            uint8_t buf[root_dir[i].size];
            read_data(blk_no, buf, root_dir[i].size);
            std::cout << buf << std::endl;
            break;
        }
    }
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "name\tsize\n";
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (root_dir[i].first_blk != 0) {
            std::cout << root_dir[i].file_name << "\t" << root_dir[i].size << std::endl;
        }
    }
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::string source_filename;
    std::string source_dir;
    std::string dest_filename;
    std::string dest_dir;
    get_filename_parts(sourcepath, &source_filename, &source_dir);
    get_filename_parts(destpath, &dest_filename, &dest_dir);

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

    uint8_t buf[new_file.size];
    read_data(current_blk, buf, new_file.size);
    
    int new_blk_no = find_empty_block();
    if (new_blk_no == -1) {
        std::cout << "No free blocks\n";
        return -1;
    }
    new_file.first_blk = new_blk_no;
    write_data(new_file.first_blk, std::string((char*)buf));

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
    
    std::string source_filename, source_dir, dest_filename, dest_dir;
    get_filename_parts(sourcepath, &source_filename, &source_dir);
    get_filename_parts(destpath, &dest_filename, &dest_dir);
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(root_dir[i].file_name, source_filename.c_str(), 56) == 0) {
            strncpy(root_dir[i].file_name, destpath.substr(destpath.find_last_of("/") + 1).c_str(), 56);
            break;
        }
    }
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";

    std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
    std::string dirpath = filepath.substr(0, filepath.find_last_of("/"));
    get_filename_parts(filepath, &filename, &dirpath);
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
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    std::string source_filename, source_dir, dest_filename, dest_dir;
    get_filename_parts(filepath1, &source_filename, &source_dir);
    get_filename_parts(filepath2, &dest_filename, &dest_dir);

    dir_entry file1;
    dir_entry *file2;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(root_dir[i].file_name, source_filename.c_str(), 56) == 0) {
            file1 = root_dir[i];
        }
        if (strncmp(root_dir[i].file_name, dest_filename.c_str(), 56) == 0) {
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


    int file2_last_blk_size = file2->size % BLOCK_SIZE;
    int new_size = file1.size + file2->size;
    std::cout << new_size << std::endl;
    file2->size = new_size - 1;
    
    int file2_last_blk = file2->first_blk;
    while (fat[file2_last_blk] != FAT_EOF) {
        file2_last_blk = fat[file2_last_blk];
    }

    std::cout << file1.size + file2_last_blk_size << std::endl;
    uint8_t buf[file1.size + file2_last_blk_size - 1]; // File one + last block of file two without null terminator
    std::cout << file2_last_blk_size << std::endl;
    read_data(file2_last_blk, buf, file2_last_blk_size - 1);
    read_data(file1.first_blk, &buf[file2_last_blk_size - 1], file1.size);
    std::cout << std::string((char*)buf) << std::endl;

    
    write_data(file2_last_blk, std::string((char*)buf));
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
