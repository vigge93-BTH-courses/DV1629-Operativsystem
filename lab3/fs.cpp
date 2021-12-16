#include <iostream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <algorithm>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    disk.read(FAT_BLOCK, (uint8_t*)fat);
    disk.read(ROOT_BLOCK, (uint8_t*)root_dir);
    memcpy(cwd, root_dir, sizeof(root_dir));
    cwd_info = cwd[0];
    cwd_blk = ROOT_BLOCK;
}

FS::~FS()
{
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    disk.write(cwd_blk, (uint8_t*)cwd);
}

void
FS::get_filename_parts(std::string filepath, std::string *filename, std::string *dirpath) {
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));
    if (find_dir_from_path(filepath) == -1) {
        *filename = filepath.substr(filepath.find_last_of("/") + 1);
        if (filepath.find_last_of("/") == std::string::npos) {
            *dirpath = get_pwd_string();
        } else {
            *dirpath = filepath.substr(0, filepath.find_last_of("/") + 1);
        }
    } else {
        *filename = "";
        *dirpath = filepath;
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
        std::cout << "No free blocks" << std::endl;
        return -1;
    }
    return blk_no;
}

int
FS::write_data(int starting_block, std::string data) {
    int blk_no = starting_block;
    while(data.length() > 0) {
        int write = disk.write(blk_no, (uint8_t*)data.c_str());
        if (write == -1) {
            std::cout << "Error writing to disk" << std::endl;
            return -1;
        }
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
    int current_blk = start_blk;
    int i = 0;
    uint8_t buf[BLOCK_SIZE];
    while (current_blk != FAT_EOF && i < size) {
        int read = disk.read(current_blk, buf);
        if (read == -1) {
            std::cout << "Error reading disk " << std::endl;
            return -1;
        }
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
    dir[0].access_rights = READ | WRITE | EXECUTE;
    return 0;
}

int
FS::find_dir_from_path(std::string dirpath) {
    if (dirpath.empty()) {
        return 0;
    }
    if (cwd_blk == ROOT_BLOCK) {
        memcpy(root_dir, cwd, sizeof(cwd));
    }

    std::string current_dir_name;
    int current_blk = -1;
    struct dir_entry current_dir[BLOCK_SIZE/sizeof(dir_entry)];
    struct dir_entry new_cwd_info;
    if (dirpath.rfind('/', 0) == 0) { // Absolute path
        dirpath.erase(0, 1);
        current_blk = ROOT_BLOCK;
        memcpy(current_dir, root_dir, sizeof(root_dir));
        new_cwd_info = root_dir[0]; // root dir info is ..
    } else { // Relative path
        current_blk = cwd_blk;
        memcpy(current_dir, cwd, sizeof(cwd));
    }

    int str_pos = 0;
    while (str_pos != std::string::npos) {
        str_pos = dirpath.find("/", str_pos);
        current_dir_name = dirpath.substr(0, str_pos);
        int found = 0;
        for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
            if (current_dir_name.empty() || strncmp(current_dir_name.c_str(), current_dir[i].file_name, 56) == 0 && current_dir[i].type == TYPE_DIR) {
                current_blk = current_dir[i].first_blk;
                new_cwd_info = current_dir[i];
                int read = disk.read(current_blk, (uint8_t*)current_dir);
                if (read == -1) {
                    std::cout << "Error reading from disk" << std::endl;
                    return -1;
                }
                found = 1;
                break;
            }
        }
        if (found == 0) {
            return -1;
        }
        dirpath.erase(0, str_pos + 1);
    }
    return 0;
}

int
FS::change_cwd(std::string dirpath) {
    if (dirpath.empty()) {
        return 0;
    }
    int write = disk.write(cwd_blk, (uint8_t*) cwd);
    if (write == -1) {
        std::cout << "Error writing to disk" << std::endl;
        return -1;
    }
    if (cwd_blk == ROOT_BLOCK) {
        memcpy(root_dir, cwd, sizeof(cwd));
    }

    std::string current_dir_name;
    int current_blk = -1;
    struct dir_entry current_dir[BLOCK_SIZE/sizeof(dir_entry)];
    struct dir_entry new_cwd_info;
    if (dirpath.rfind('/', 0) == 0) { // Absolute path
        dirpath.erase(0, 1);
        current_blk = ROOT_BLOCK;
        memcpy(current_dir, root_dir, sizeof(root_dir));
        new_cwd_info = root_dir[0]; // root dir info is ..
    } else { // Relative path
        current_blk = cwd_blk;
        memcpy(current_dir, cwd, sizeof(cwd));
    }

    int str_pos = 0;
    while (str_pos != std::string::npos) {
        str_pos = dirpath.find("/", str_pos);
        current_dir_name = dirpath.substr(0, str_pos);
        int found = 0;
        for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
            if (current_dir_name.empty() || strncmp(current_dir_name.c_str(), current_dir[i].file_name, 56) == 0 && current_dir[i].type == TYPE_DIR) {
                current_blk = current_dir[i].first_blk;
                new_cwd_info = current_dir[i];
                int read = disk.read(current_blk, (uint8_t*)current_dir);
                if (read == -1) {
                    std::cout << "Error reading from disk" << std::endl;
                    return -1;
                }
                found = 1;
                break;
            }
        }
        if (found == 0) {
            std::cout << "Path not found" << std::endl;
            return -1;
        }
        dirpath.erase(0, str_pos + 1);
    }
    cwd_info = new_cwd_info;
    cwd_blk = current_blk;
    memcpy(cwd, current_dir, sizeof(current_dir));
    return 0;
}

int
FS::restore_cwd(dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)], int cur_cwd_blk, dir_entry cur_cwd_info, int save = 1) {
    if (save == 0) {
        memcpy(cwd, cur_cwd, sizeof(cwd));
        cwd_info = cur_cwd_info;
        cwd_blk = cur_cwd_blk;
    } else if (cwd_blk != cur_cwd_blk) {
        if (disk.write(cwd_blk, (uint8_t*) cwd) == -1) {
            std::cout << "Fatal error when restoring cwd. Exiting." << std::endl;
            exit(1);
        };

        cwd_blk = cur_cwd_blk;
        cwd_info = cur_cwd_info;
        memcpy(cwd, cur_cwd, sizeof(cwd));
    }
    if (cur_cwd_blk == ROOT_BLOCK) {
        memcpy(root_dir, cur_cwd, sizeof(root_dir));
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

    init_dir(root_dir, ROOT_BLOCK);
    memcpy(cwd, root_dir, sizeof(root_dir));
    cwd_blk = 0;
    cwd_info = root_dir[0];
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    dir_entry file;
    std::string filename;
    std::string dirpath;
    get_filename_parts(filepath, &filename, &dirpath);
    if (filename.empty()) {
        std::cout << "Cannot create file with same name as a directory." << std::endl;
        return -1;
    }
    if (change_cwd(dirpath) == -1) {
        return -1;
    }
    if ((cwd_info.access_rights & WRITE) == 0  || (cwd_info.access_rights & EXECUTE) == 0) {
        std::cout << "You do not have permission to create files in this directory." << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }

    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, filename.c_str(), 56) == 0) {
            std::cout << "A file or directory named " << filename << " already exists in this directory." << std::endl;
            restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
            return -1;
        }
    }

    std::string data;
    std::string in;
    while (std::getline(std::cin, in)) {
        if (in.empty())
            break;
        data.append(in + '\n');
    }

    int size = data.length() + 1; // Include null terminator.
    strncpy(file.file_name, filename.c_str(), 56);
    file.size = size;
    file.type = TYPE_FILE;
    file.access_rights = READ | WRITE;

    int blk_no = find_empty_block();
    if (blk_no == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    file.first_blk = blk_no;

    int dir_index = -1;
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (cwd[i].first_blk == 0) {
            dir_index = i;
            break;
        }
    }
    if (dir_index == -1) {
        std::cout << "No free space in directory\n";
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }

    if (write_data(file.first_blk, data) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    cwd[dir_index] = file;

    restore_cwd(cur_cwd, cwd_blk, cur_cwd_info);
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));
    
    std::string filename;
    std::string dirpath;
    get_filename_parts(filepath, &filename, &dirpath);
    if (filename.empty()) {
        std::cout << "Filename is empty" << std::endl;
        return -1;
    }
    if (change_cwd(dirpath) == -1) {
        return -1;
    }
    
    int found = 0;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, filename.c_str(), 56) == 0) {
            if (cwd[i].type == TYPE_DIR) {
                std::cout << filename << " is not a file" << std::endl;
                restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
                return -1;
            }
            if ((cwd[i].access_rights & READ) == 0) {
                std::cout << "You do not have permission to read " << filename << std::endl;
                restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
                return -1;
            }
            int blk_no = cwd[i].first_blk;
            uint8_t buf[cwd[i].size];

            if (read_data(blk_no, buf, cwd[i].size) == -1) {
                restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
                return -1;
            }
            std::cout << buf << std::endl;
            found = 1;
            break;
        }
    }
    if (found == 0) {
        std::cout << "File \"" << filename << "\" not found" << std::endl;
    }

    restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
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
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    std::string source_filename;
    std::string source_dir;
    std::string dest_filename;
    std::string dest_dir;
    get_filename_parts(sourcepath, &source_filename, &source_dir);
    get_filename_parts(destpath, &dest_filename, &dest_dir);
    if (source_filename.empty()) {
        std::cout << "Source file not found" << std::endl;
        return -1;
    }
    if (dest_filename.empty()) {
        dest_filename = source_filename;
    }

    dir_entry new_file;
    strncpy(new_file.file_name, dest_filename.c_str(), 56);
    new_file.first_blk = 0;

    if (change_cwd(source_dir) == -1) {
        return -1;
    }
    int current_blk = -1;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, source_filename.c_str(), 56) == 0) {
            if ((cwd[i].access_rights & READ) == 0) {
                std::cout << "You do not have permission to read " << source_filename << std::endl;
                restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
                return -1;
            }
            new_file.size = cwd[i].size;
            new_file.type = cwd[i].type;
            new_file.access_rights = cwd[i].access_rights;
            current_blk = cwd[i].first_blk;
            break;
        }
    }
    if (current_blk == -1) {
        std::cout << "File not found" << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }

    uint8_t buf[new_file.size];
    if (read_data(current_blk, buf, new_file.size) == -1) {
        return -1;
    }
    
    int new_blk_no = find_empty_block();
    if (new_blk_no == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    new_file.first_blk = new_blk_no;
    
    if (change_cwd(dest_dir) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }

    if (cwd_info.access_rights & WRITE == 0) {
        std::cout << "You do not have permission to create files in this directory." << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, new_file.file_name, 56) == 0) {
            std::cout << "File with name " << new_file.file_name << " already exists" << std::endl;
            restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
            return -1;
        }
    }
    int cwd_index = -1;
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (cwd[i].first_blk == 0) {
            cwd_index = i;
            break;
        }
    }
    if (cwd_index == -1) {
        std::cout << "No space in directory" << std::endl;
        return -1;
    }
    if (write_data(new_file.first_blk, std::string((char*)buf)) == -1) {
        return -1;     
    };
    cwd[cwd_index] = new_file;

    restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info);
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    std::string source_filename, source_dir, dest_filename, dest_dir;
    get_filename_parts(sourcepath, &source_filename, &source_dir);
    get_filename_parts(destpath, &dest_filename, &dest_dir);
    if (source_filename.empty()) {
        std::cout << "Source file not found" << std::endl;
        return -1;
    }
    if (dest_filename.empty()) {
        dest_filename = source_filename;
    }

    if (change_cwd(source_dir) == -1) {
        return -1;
    }
    if ((cwd_info.access_rights & WRITE) == 0 || (cwd_info.access_rights & EXECUTE) == 0) {
        std::cout << "You do not have permission to move files from this directory." << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    if (change_cwd(dest_dir) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    };
    if ((cwd_info.access_rights & WRITE) == 0 || (cwd_info.access_rights & EXECUTE) == 0) {
        std::cout << "You do not have permission to move files to this directory." << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    for(int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, dest_filename.c_str(), 56) == 0) {
            std::cout << "File with name " << dest_filename << " already exists" << std::endl;
            restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
            return -1;
        }
    }
    if (change_cwd(source_dir) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    dir_entry file;
    int found = 0;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, source_filename.c_str(), 56) == 0) {
            file = cwd[i];
            cwd[i].first_blk = 0;
            cwd[i].size = 0;
            cwd[i].type = 0;
            strncpy(cwd[i].file_name, "", 56);
            found = 1;
            break;
        }
    }
    if (found == 0) {
        std::cout << "File not found" << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    if(disk.write(cwd_blk, (uint8_t*) cwd) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    // Update saved cwd
    if (cur_cwd_blk == cwd_blk) {
        memcpy(cur_cwd, cwd, sizeof(cwd));
    }
    if(change_cwd(dest_dir) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (cwd[i].first_blk == 0) {
            cwd[i] = file;
            strncpy(cwd[i].file_name, dest_filename.c_str(), 56);
            break;
        }
    }

    restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info);
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
    std::string dirpath = filepath.substr(0, filepath.find_last_of("/"));
    get_filename_parts(filepath, &filename, &dirpath);
    if (filename.empty()) {
        std::cout << "File not found" << std::endl;
        return -1;
    }

    if (change_cwd(dirpath) == -1) {
        return -1;
    }
    if ((cwd_info.access_rights & WRITE) == 0 || (cwd_info.access_rights & EXECUTE) == 0) {
        std::cout << "You do not have permission to remove files in this directory." << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    int block_no = -1;
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) { // Can't remove ..;
        if (strncmp(cwd[i].file_name, filename.c_str(), 56) == 0) {
            if (cwd[i].type == TYPE_DIR) {
                std::cout << "Can't remove directories" << std::endl;
                restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
                return -1;
            }
            block_no = cwd[i].first_blk;
            cwd[i].first_blk = 0;
            cwd[i].size = 0;
            memset(cwd[i].file_name, 0, 56);
            break;
        }
    }
    if (block_no == -1) {
        std::cout << "File not found" << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }

    while (block_no != FAT_EOF) {
        int next_blk = fat[block_no];
        fat[block_no] = FAT_FREE;
        block_no = next_blk;
    }
    restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info);
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    std::string source_filename, source_dir, dest_filename, dest_dir;
    get_filename_parts(filepath1, &source_filename, &source_dir);
    get_filename_parts(filepath2, &dest_filename, &dest_dir);
    if (source_filename.empty() || dest_filename.empty()) {
        std::cout << "File not found" << std::endl;
        return -1;
    }

    dir_entry file1;
    dir_entry *file2;
    if(change_cwd(source_dir) == -1) {
        return -1;
    }
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, source_filename.c_str(), 56) == 0) {
            file1 = cwd[i];
        }
    }

    if (change_cwd(dest_dir) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, dest_filename.c_str(), 56) == 0) {
            file2 = &cwd[i];
        }
    }

    if (file1.first_blk == 0 || file2->first_blk == 0) {
        std::cout << "File not found\n";
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    if ((file1.access_rights & READ) == 0) {
        std::cout << "You do not have permission to read " << source_filename << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    if ((file2->access_rights & READ) == 0 || (file2->access_rights & WRITE) == 0) {
        std::cout << "You do not have permission to read or write " << dest_filename << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
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
    if (read_data(file2_last_blk, buf, file2_last_blk_size - 1) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    if (read_data(file1.first_blk, &buf[file2_last_blk_size - 1], file1.size) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    };

    if (write_data(file2_last_blk, std::string((char*)buf)) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    };

    restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info);
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));
    
    std::string dirname;
    std::string path;
    get_filename_parts(dirpath, &dirname, &path);
    if (dirname.empty()) {
        std::cout << "Directory already exists" << std::endl;
        return -1;
    }
    if (change_cwd(path) == -1) {
        return -1;
    }
    if ((cwd_info.access_rights & WRITE) == 0 || (cwd_info.access_rights & EXECUTE) == 0) {
        std::cout << "You do not have permission to create directories in this directory." << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, dirname.c_str(), 56) == 0) {
            std::cout << "A file or directory named " << dirname << " already exists in this directory." << std::endl;
            restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
            return -1;
        }
    }
    dir_entry new_entry;
    strncpy(new_entry.file_name, dirname.c_str(), 56);
    new_entry.size = 0;
    new_entry.first_blk = find_empty_block();
    if (new_entry.first_blk == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    new_entry.type = TYPE_DIR;
    new_entry.access_rights = READ | WRITE | EXECUTE;
    int dir_index = -1;
    for (int i = 1; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (cwd[i].first_blk == 0) {
            dir_index = i;
            break;
        }
    }
    if (dir_index == -1) {
        std::cout << "No free space in directory\n";
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    
    dir_entry new_dir[BLOCK_SIZE/sizeof(dir_entry)];
    init_dir(new_dir, cwd_blk);
    if (disk.write(new_entry.first_blk, (uint8_t*) new_dir) == -1) {
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    };

    cwd[dir_index] = new_entry;
    fat[new_entry.first_blk] = FAT_EOF;

    restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info);
    return 0;   
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));

    if (change_cwd(dirpath) == -1) {
        return -1;
    }
    if ((cwd_info.access_rights & READ) == 0) {
        std::cout << "Permisssion denied" << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << get_pwd_string() << std::endl;
    return 0;
}

std::string
FS::get_pwd_string()
{
    std::vector<std::string> dirs;
    std::string res = "";
    struct dir_entry current_dir;
    int current_blk = cwd_blk;
    if (current_blk == ROOT_BLOCK) {
        return "/";
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
        res.append("/" + dirs.back());
        dirs.pop_back();
    }
    return res;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    int cur_cwd_blk = cwd_blk;
    dir_entry cur_cwd_info = cwd_info;
    dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)];
    memcpy(cur_cwd, cwd, sizeof(cwd));
    
    std::string filename;
    std::string dirname;

    int access_level = atoi(accessrights.c_str());
    get_filename_parts(filepath, &filename, &dirname);
    if (filename.empty()) {
        filename = dirname.substr(dirname.find_last_of("/") + 1);
        dirname = dirname.substr(0, dirname.find_last_of("/") + 1);
    }

    change_cwd(dirname);
    int found = 0;
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        if (strncmp(cwd[i].file_name, filename.c_str(), 56) == 0) {
            cwd[i].access_rights = access_level;
            if (cwd_blk == cwd[i].first_blk) {
                cwd_info.access_rights = access_level;
            }
            found = 1;
        }
    }
    if (found == 0) {
        std::cout << "File or directory " << filename << " not found" << std::endl;
        restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info, 0);
        return -1;
    }
    restore_cwd(cur_cwd, cur_cwd_blk, cur_cwd_info);
    return 0;
}
