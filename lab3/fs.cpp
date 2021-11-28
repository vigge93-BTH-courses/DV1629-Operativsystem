#include <iostream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <algorithm>
#include "fs.h"

// TODO: Don't restore cwd if cwd is modified

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    disk.read(FAT_BLOCK, (uint8_t*)fat);
    disk.read(ROOT_BLOCK, (uint8_t*)root_dir);
    memcpy(cwd, root_dir, sizeof(root_dir));
    working_dir_blk = ROOT_BLOCK;
}

FS::~FS()
{
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    disk.write(working_dir_blk, (uint8_t*)cwd);
}

void
FS::get_filename_parts(std::string filepath, std::string *filename, std::string *dirpath) {
    *filename = filepath.substr(filepath.find_last_of("/") + 1);
    if (filepath.find_last_of("/") == std::string::npos) {
        *dirpath = "";
    } else {
        *dirpath = filepath.substr(0, filepath.find_last_of("/"));
    }
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

int
FS::init_dir(struct dir_entry *dir, int parent_blk) {
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        memset(dir[i].file_name, 0, 56);
        dir[i].size = 0;
        dir[i].first_blk = 0;
        dir[i].type = TYPE_FILE;
        dir[i].access_rights = 0;
    }
    strncpy(dir[0].file_name, "..", 2);
    dir[0].first_blk = parent_blk;
    dir[0].type = TYPE_DIR;
    dir[0].access_rights = READ | WRITE;
    return 0;
}

int
FS::change_cwd(std::string dirpath) {
    if (dirpath.empty()) {
        return 0;
    }
    disk.write(working_dir_blk, (uint8_t*) cwd);

    std::string current_dir_name;
    int str_pos = 0;
    int current_blk = -1;
    struct dir_entry current_dir[BLOCK_SIZE/sizeof(dir_entry)];
    if (dirpath.rfind('/', 0) == 0) { // Absolute path
        dirpath.erase(0, 1);
        current_blk = ROOT_BLOCK;
        memcpy(current_dir, root_dir, sizeof(root_dir));
    } else { // Relative path
        current_blk = working_dir_blk;
        memcpy(current_dir, cwd, sizeof(cwd));
    }
    while (str_pos != std::string::npos) {
        str_pos = dirpath.find("/", str_pos);
        current_dir_name = dirpath.substr(0, str_pos);
        for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
            if (strncmp(current_dir_name.c_str(), current_dir[i].file_name, 56) == 0 && current_dir[i].type == TYPE_DIR) {
                current_blk = current_dir[i].first_blk;
                disk.read(current_blk, (uint8_t*)current_dir);
            }
        }
        dirpath.erase(0, str_pos + 1);
    }
    memcpy(cwd, current_dir, sizeof(current_dir));
    working_dir_blk = current_blk;
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

    init_dir(root_dir, ROOT_BLOCK);
    memcpy(cwd, root_dir, sizeof(root_dir));
    working_dir_blk = 0;
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    int cwd_blk = working_dir_blk;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    dir_entry file;
    std::string filename;
    std::string dirpath;
    get_filename_parts(filepath, &filename, &dirpath);
    change_cwd(dirpath);

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
    file.type = TYPE_FILE;
    file.access_rights = READ | WRITE;

    int blk_no = find_empty_block();
    if (blk_no == -1) {
        std::cout << "No free blocks\n";
        return -1;
    }
    file.first_blk = blk_no;

    int dir_index = -1;
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (cwd[i].first_blk == 0) {
            cwd[i] = file;
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

    // Restore cwd
    if (working_dir_blk != cwd_blk) {
        disk.write(working_dir_blk, (uint8_t*) cwd);
        working_dir_blk = cwd_blk;
        disk.read(cwd_blk, (uint8_t*)cwd);
    }
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    int cwd_blk = working_dir_blk;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));
    
    std::string filename;
    std::string dirpath;
    get_filename_parts(filepath, &filename, &dirpath);
    change_cwd(dirpath);
    
    int found = 0;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, filename.c_str(), 56) == 0) {
            int blk_no = cwd[i].first_blk;
            uint8_t buf[cwd[i].size];
            read_data(blk_no, buf, cwd[i].size);
            std::cout << buf << std::endl;
            found = 1;
            break;
        }
    }
    if (found == 0) {
        std::cout << "File \"" << filename << "\" not found" << std::endl;
    }

    // Restore cwd
    working_dir_blk = cwd_blk;
    memcpy(cwd, cur_cwd, sizeof(cwd));
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << std::left;
    std::cout << std::setw(56) << "name" << "\ttype\taccessrights\tsize\n";
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (cwd[i].first_blk != 0 || cwd[i].type == TYPE_DIR) {
            std::string type_name;
            if (cwd[i].type == TYPE_DIR) type_name = "Dir";
            else type_name = "File";
            std::cout << std::setw(56) << cwd[i].file_name << "\t" << type_name << "\t";
            std::cout << (((cwd[i].access_rights & READ) != 0) ? "r" : "-");
            std::cout << (((cwd[i].access_rights & WRITE) != 0) ? "w" : "-");
            std::cout << (((cwd[i].access_rights & EXECUTE) != 0) ? "e" : "-");
            std::cout << "\t\t" << (cwd[i].type == TYPE_DIR ? "-" : std::to_string(cwd[i].size)) << std::endl;
        }
    }
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    int cwd_blk = working_dir_blk;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    std::string source_filename;
    std::string source_dir;
    std::string dest_filename;
    std::string dest_dir;
    get_filename_parts(sourcepath, &source_filename, &source_dir);
    get_filename_parts(destpath, &dest_filename, &dest_dir);

    dir_entry new_file;
    strncpy(new_file.file_name, dest_filename.c_str(), 56);
    new_file.first_blk = 0;

    change_cwd(source_dir);
    int current_blk = -1;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, source_filename.c_str(), 56) == 0) {
            new_file.size = cwd[i].size;
            new_file.type = cwd[i].type;
            new_file.access_rights = cwd[i].access_rights;
            current_blk = cwd[i].first_blk;
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

    change_cwd(dest_dir);
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (cwd[i].first_blk == 0) {
            cwd[i] = new_file;
            break;
        }
    }

    // Restore cwd
    disk.write(working_dir_blk, (uint8_t*) cwd);
    working_dir_blk = cwd_blk;
    memcpy(cwd, cur_cwd, sizeof(cwd));
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    int cwd_blk = working_dir_blk;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    std::string source_filename, source_dir, dest_filename, dest_dir;
    get_filename_parts(sourcepath, &source_filename, &source_dir);
    get_filename_parts(destpath, &dest_filename, &dest_dir);

    change_cwd(source_dir);
    dir_entry file;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, source_filename.c_str(), 56) == 0) {
            file = cwd[i];
            cwd[i].first_blk = 0;
            cwd[i].size = 0;
            cwd[i].type = 0;
            strncpy(cwd[i].file_name, "", 56);
        }
    }
    disk.write(working_dir_blk, (uint8_t*) cwd);
    change_cwd(dest_dir);
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (cwd[i].first_blk == 0) {
            cwd[i] = file;
        }
    }

    // Restore cwd
    disk.write(working_dir_blk, (uint8_t*) cwd);
    working_dir_blk = cwd_blk;
    memcpy(cwd, cur_cwd, sizeof(cwd));
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    int cwd_blk = working_dir_blk;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
    std::string dirpath = filepath.substr(0, filepath.find_last_of("/"));
    get_filename_parts(filepath, &filename, &dirpath);
    change_cwd(dirpath);

    int block_no = 0;
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) { // Can't remove ..;
        if (strncmp(cwd[i].file_name, filename.c_str(), 56) == 0) {
            block_no = cwd[i].first_blk;
            cwd[i].first_blk = 0;
            cwd[i].size = 0;
            memset(cwd[i].file_name, 0, 56);
            break;
        }
    }
    while (block_no != FAT_EOF) {
        int next_blk = fat[block_no];
        fat[block_no] = FAT_FREE;
        block_no = next_blk;
    }
    // Restore cwd
    disk.write(working_dir_blk, (uint8_t*) cwd);
    working_dir_blk = cwd_blk;
    memcpy(cwd, cur_cwd, sizeof(cwd));
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    int cwd_blk = working_dir_blk;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    std::string source_filename, source_dir, dest_filename, dest_dir;
    get_filename_parts(filepath1, &source_filename, &source_dir);
    get_filename_parts(filepath2, &dest_filename, &dest_dir);

    dir_entry file1;
    dir_entry *file2;
    change_cwd(source_dir);
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, source_filename.c_str(), 56) == 0) {
            file1 = cwd[i];
        }
    }
    change_cwd(dest_dir);
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, dest_filename.c_str(), 56) == 0) {
            file2 = &cwd[i];
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
    file2->size = new_size - 1;
    
    int file2_last_blk = file2->first_blk;
    while (fat[file2_last_blk] != FAT_EOF) {
        file2_last_blk = fat[file2_last_blk];
    }

    uint8_t buf[file1.size + file2_last_blk_size - 1]; // File one + last block of file two without null terminator
    read_data(file2_last_blk, buf, file2_last_blk_size - 1);
    read_data(file1.first_blk, &buf[file2_last_blk_size - 1], file1.size);

    write_data(file2_last_blk, std::string((char*)buf));

    // Restore cwd
    disk.write(working_dir_blk, (uint8_t*) cwd);
    working_dir_blk = cwd_blk;
    memcpy(cwd, cur_cwd, sizeof(cwd));
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    int cwd_blk = working_dir_blk;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));
    
    std::string dirname;
    std::string path;
    get_filename_parts(dirpath, &dirname, &path);
    change_cwd(path);

    dir_entry new_entry;
    strncpy(new_entry.file_name, dirname.c_str(), 56);
    new_entry.size = 0;
    new_entry.first_blk = find_empty_block();
    if (new_entry.first_blk == -1) {
        std::cout << "No free blocksn\n" <<std::endl;
        return -1;
    }
    new_entry.type = TYPE_DIR;
    new_entry.access_rights = READ | WRITE;
    int dir_index = -1;
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (cwd[i].first_blk == 0) {
            dir_index = i;
            break;
        }
    }
    if (dir_index == -1) {
        std::cout << "No free space in directory\n";
        return -1;
    }
    cwd[dir_index] = new_entry;
    fat[new_entry.first_blk] = FAT_EOF;
    
    dir_entry new_dir[BLOCK_SIZE/sizeof(dir_entry)];
    std::cout << new_entry.first_blk << std::endl;
    init_dir(new_dir, working_dir_blk);
    std::cout << sizeof(new_dir) << std::endl;
    disk.write(new_entry.first_blk, (uint8_t*) new_dir);
    // Restore cwd
    disk.write(working_dir_blk, (uint8_t*) cwd);
    working_dir_blk = cwd_blk;
    memcpy(cwd, cur_cwd, sizeof(cwd));
    return 0;   
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    change_cwd(dirpath);
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    std::vector<std::string> dirs;
    struct dir_entry current_dir;
    int current_blk = working_dir_blk;
    if (current_blk == ROOT_BLOCK) {
        std::cout << "/" << std::endl;
        return 0;
    }
    int prev_blk = cwd[0].first_blk;
    while (current_blk != ROOT_BLOCK) {
        struct dir_entry prev_dir[BLOCK_SIZE/sizeof(dir_entry)];
        disk.read(prev_blk, (uint8_t*)prev_dir);
        for(int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
            if (prev_dir[i].first_blk == current_blk) {
                dirs.push_back(prev_dir[i].file_name);
                break;
            }
        }
        current_blk = prev_blk;
        prev_blk = prev_dir[0].first_blk;
    }
    while(dirs.size() > 0) {
        std::cout << "/" << dirs.back();
        dirs.pop_back();
    }
    std::cout << std::endl;
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    int cwd_blk = working_dir_blk;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));
    
    std::string filename;
    std::string dirname;

    int access_level = atoi(accessrights.c_str());
    get_filename_parts(filepath, &filename, &dirname);
    change_cwd(dirname);
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, filename.c_str(), 56) == 0) {
            cwd[i].access_rights = access_level;
        }
    }
    // Restore cwd
    disk.write(working_dir_blk, (uint8_t*) cwd);
    working_dir_blk = cwd_blk;
    memcpy(cwd, cur_cwd, sizeof(cwd));
    return 0;
}
