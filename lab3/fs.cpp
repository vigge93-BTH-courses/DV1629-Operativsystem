#include <iostream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    disk.read(FAT_BLOCK, (uint8_t*)fat);
    disk.read(ROOT_BLOCK, (uint8_t*)root_dir);
    memcpy(cwd.entries, root_dir, sizeof(root_dir));
    cwd.info = root_dir[PARENT_DIR_ENTRY_INDEX];
    cwd.blk = ROOT_BLOCK;
}

FS::~FS()
{
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    disk.write(cwd.blk, (uint8_t*)cwd.entries);
}

void
FS::get_filename_parts(std::string filepath, std::string *filename, std::string *dirpath) {
    if (find_dir_from_path(filepath) == -1) {
        *filename = filepath.substr(filepath.find_last_of("/") + 1);
        if (filepath.rfind("/", 0) == std::string::npos) { // Convert relative path to absolute path
            *dirpath = get_pwd_string() + "/" + filepath.substr(0, filepath.find_last_of("/") + 1);
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
    for(int i = 0; i < FAT_ENTRIES; i++) {
        if (fat[i] == FAT_FREE) {
            blk_no = i;
            break;
        }
    }
    if (blk_no == -1) {
        std::cerr << "FS::find_empty_block: No free blocks" << std::endl;
        return -1;
    }
    return blk_no;
}

int
FS::write_data(int starting_block, std::string data) {
    int blk_no = starting_block;

    int write = disk.write(blk_no, (uint8_t*)data.c_str());
    if (write == -1) {
        std::cerr << "FS::write_data: Error writing block " << blk_no << " to disk" << std::endl;
        return -1;
    }
    fat[blk_no] = FAT_EOF;

    if (DEBUG) {
        std::cout << "FS::write_data: data size: " << data.length() << ", block: no: " << blk_no << std::endl;
    }
    while(data.length() >= BLOCK_SIZE) {
        data = data.substr(BLOCK_SIZE);
        
        int prev_blk_no = blk_no;
        int blk_no = find_empty_block();
        if (blk_no == -1) {
            std::cerr << "FS::write_data: Failed to find empty block" << std::endl;
            return -1;
        }
        fat[prev_blk_no] = blk_no;        

        int write = disk.write(blk_no, (uint8_t*)data.c_str());
        if (write == -1) {
            std::cerr << "FS::write_data: Error writing block " << blk_no << " to disk" << std::endl;
            return -1;
        }
    
        fat[blk_no] = FAT_EOF;
        
        if (DEBUG) {
            std::cout << "FS::write_data: data size: " << data.length() << ", block: no: " << blk_no << std::endl;
        }
    }
    return 0;
}

int
FS::read_data(int start_blk, uint8_t* out_buf, size_t size) { 
    int current_blk = start_blk;
    size_t bytes_read = 0;
    uint8_t buf[BLOCK_SIZE];
    while (current_blk != FAT_EOF && bytes_read < size) {
        // Ensure 0 <= bytes_to_read <= BLOCK_SIZE
        size_t bytes_to_read = std::max(std::min((ulong)BLOCK_SIZE, size - bytes_read), 0ul);
        if (DEBUG) {
            std::cout << "FS:read_data: size: " << size << ", bytes_to_read: " << bytes_to_read << ", bytes_read: " << bytes_read << std::endl;
        }
        
        int read = disk.read(current_blk, buf);
        if (read == -1) {
            std::cerr << "FS::read_data: Error reading block " << current_blk << "from disk" << std::endl;
            return -1;
        }
        
        memcpy(&out_buf[bytes_read], buf, bytes_to_read);
        bytes_read += bytes_to_read;

        current_blk = fat[current_blk];
    }
    return 0;
}

int
FS::init_dir(struct dir_entry *dir, int parent_blk, uint8_t parent_access_rights) {
    for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
        memset(dir[i].file_name, 0, 56);
        dir[i].size = 0;
        dir[i].first_blk = 0;
        dir[i].type = TYPE_FILE;
        dir[i].access_rights = 0;
    }
    strncpy(dir[PARENT_DIR_ENTRY_INDEX].file_name, "..", 3);
    dir[PARENT_DIR_ENTRY_INDEX].first_blk = parent_blk;
    dir[PARENT_DIR_ENTRY_INDEX].type = TYPE_DIR;
    dir[PARENT_DIR_ENTRY_INDEX].access_rights = parent_access_rights;
    return 0;
}

dir_entry*
FS::find_dir_entry(std::string filename) {
    int current_blk = -1;
    for (int i = 0; i < DIR_SIZE; i++) {
        if (strncmp(cwd.entries[i].file_name, filename.c_str(), 56) == 0) {
            return &cwd.entries[i];
        }
    }
    if (DEBUG) {
        std::cout << "FS::find_dir_entry: entry \"" << filename << "\" not found in " << cwd.info.file_name << std::endl;
    }
    return nullptr;
}

int
FS::find_empty_dir_index() {
    int dir_index = -1;
    for (int i = 1; i < DIR_SIZE; i++) {
        if (cwd.entries[i].first_blk == FAT_FREE) {
            dir_index = i;
            break;
        }
    }
    if (dir_index == -1) {
        std::cerr << "FS::find_empty_dir_index: No free space in directory" << cwd.info.file_name << std::endl;
        return -1;
    }
    return dir_index;
}

int
FS::find_dir_from_path(std::string dirpath) {
    if (dirpath.empty()) {
        return 0;
    }
    if (cwd.blk == ROOT_BLOCK) {
        memcpy(root_dir, cwd.entries, sizeof(cwd.entries));
    }

    std::string current_dir_name;
    int current_blk = -1;
    struct dir_entry current_dir[DIR_SIZE];
    if (dirpath.rfind('/', 0) == 0) { // Absolute path
        dirpath.erase(0, 1);
        current_blk = ROOT_BLOCK;
        memcpy(current_dir, root_dir, sizeof(root_dir));
    } else { // Relative path
        current_blk = cwd.blk;
        memcpy(current_dir, cwd.entries, sizeof(cwd.entries));
    }

    int str_pos = 0;
    while (str_pos != std::string::npos) {
        str_pos = dirpath.find("/", str_pos);
        current_dir_name = dirpath.substr(0, str_pos);
        int found = 0;
        for (int i = 0; i < DIR_SIZE; i++) {
            if (current_dir_name.empty() || strncmp(current_dir_name.c_str(), current_dir[i].file_name, 56) == 0 && current_dir[i].type == TYPE_DIR) {
                current_blk = current_dir[i].first_blk;
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
    int write = disk.write(cwd.blk, (uint8_t*) cwd.entries);
    if (write == -1) {
        std::cout << "Error writing to disk" << std::endl;
        return -1;
    }
    if (cwd.blk == ROOT_BLOCK) {
        memcpy(root_dir, cwd.entries, sizeof(cwd.entries));
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
        current_blk = cwd.blk;
        memcpy(current_dir, cwd.entries, sizeof(cwd.entries));
        new_cwd_info = cwd.info;
    }

    int str_pos = 0;
    while (str_pos != std::string::npos) {
        str_pos = dirpath.find("/", str_pos);
        current_dir_name = dirpath.substr(0, str_pos);
        int found = 0;
        for (int i = 0; i < BLOCK_SIZE/sizeof(dir_entry); i++) {
            if (current_dir_name.empty()) {
                found = 1;
                break;
            }
            if (strncmp(current_dir_name.c_str(), current_dir[i].file_name, 56) == 0 && current_dir[i].type == TYPE_DIR) {
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
    cwd.info = new_cwd_info;
    cwd.blk = current_blk;
    memcpy(cwd.entries, current_dir, sizeof(current_dir));
    return 0;
}

int
FS::exit_method(bool save = false) {
    if (!save) {
        memcpy(cwd.entries, cwd_backup.entries, sizeof(cwd.entries));
        cwd.info = cwd_backup.info;
        cwd.blk = cwd_backup.blk;
        
    } else {
        if (cwd.blk != cwd_backup.blk) {
            if (disk.write(cwd.blk, (uint8_t*) cwd.entries) == -1) {
                std::cerr << "FS::exit_method: Fatal error when restoring cwd. Exiting." << std::endl;
                exit(1);
            };
            cwd.blk = cwd_backup.blk;
            cwd.info = cwd_backup.info;
            memcpy(cwd.entries, cwd_backup.entries, sizeof(cwd.entries));
        }
    }
    if (cwd.blk == ROOT_BLOCK) {
        memcpy(root_dir, cwd.entries, sizeof(root_dir));
    }
    return 0;
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    for (int i = 0; i < FAT_ENTRIES; i++) {
        fat[i] = FAT_FREE;
    }
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;

    init_dir(root_dir, ROOT_BLOCK, READ | WRITE | EXECUTE);
    memcpy(cwd.entries, root_dir, sizeof(root_dir));
    cwd.blk = ROOT_BLOCK;
    cwd.info = root_dir[PARENT_DIR_ENTRY_INDEX];
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    enter_method();

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
    if (!has_permission(cwd.info, WRITE | EXECUTE)) {
        std::cout << "You do not have permission to create files in this directory." << std::endl;
        return exit_method();
    }

    dir_entry* existing = find_dir_entry(filename);
    if (existing != nullptr) {
        std::cout << "A file or directory named " << filename << " already exists in this directory." << std::endl;
        return exit_method();
    }

    std::string data;
    std::string in;
    while (std::getline(std::cin, in)) {
        if (in.empty())
            break;
        data.append(in + '\n');
    }
    
    dir_entry file;
    int size = data.length() + 1; // Include null terminator.
    strncpy(file.file_name, filename.c_str(), 56);
    file.size = size;
    file.type = TYPE_FILE;
    file.access_rights = READ | WRITE;

    if (DEBUG) {
        std::cout << "FS::create: Data length: " << data.length() << ", size: " << size << std::endl;
    }
    
    int blk_no = find_empty_block();
    if (blk_no == -1) {
        return exit_method();
    }
    file.first_blk = blk_no;


    int dir_index = find_empty_dir_index();
    if (dir_index == -1) {
        std::cerr << "FS::create: No free space in directory\n";
        exit_method();
        return -1;
    }

    if (write_data(file.first_blk, data) == -1) {
        exit_method();
        return -1;
    }
    cwd.entries[dir_index] = file;

    exit_method(true);
    return 0;
}

int
FS::enter_method() {
    cwd_backup.blk = cwd.blk;
    cwd_backup.info = cwd.info;
    memcpy(cwd_backup.entries, cwd.entries, sizeof(cwd.entries));
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    enter_method();
    
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
    
    dir_entry* file = find_dir_entry(filename);
    if (file == nullptr) {
        std::cout << "File \"" << filename << "\" not found" << std::endl;
        return exit_method();
    }
    if (file->type == TYPE_DIR) {
        std::cout << filename << " is not a file" << std::endl;
        return exit_method();
    }
    if (!has_permission(*file, READ))  {
        std::cout << "You do not have permission to read " << filename << std::endl;
        return exit_method();
    }
    int blk_no = file->first_blk;
    uint8_t buf[file->size];

    if (read_data(blk_no, buf, file->size) == -1) {
        exit_method();
        return -1;
    }
    
    std::cout << buf << std::endl;
    exit_method();
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    if (!has_permission(cwd.info, READ)) {
        std::cout << "You do not have permissions to read the contents of this directory" << std::endl;
        return 0;
    }
    std::cout << std::left;
    std::cout << std::setw(56) << "name" << "\ttype\taccessrights\tsize\n";
    for (int i = 0; i < DIR_SIZE; i++) {
        dir_entry entry = cwd.entries[i];
        if (!dir_entry_is_empty(entry)) {
            std::string type_name = entry.type == TYPE_DIR ? "Dir" : "File";
            std::cout << std::setw(56) << entry.file_name << "\t" << type_name << "\t";
            std::cout << (((entry.access_rights & READ) != 0) ? "r" : "-");
            std::cout << (((entry.access_rights & WRITE) != 0) ? "w" : "-");
            std::cout << (((entry.access_rights & EXECUTE) != 0) ? "e" : "-");
            std::cout << "\t\t" << (entry.type == TYPE_DIR ? "-" : std::to_string(entry.size)) << std::endl;
        }
    }
    return 0;
}

bool
FS::dir_entry_is_empty(dir_entry entry) {
    return entry.first_blk == FAT_FREE && entry.type == TYPE_FILE;
}

bool FS::has_permission(dir_entry entry, uint8_t required_access_rights) {
    return (entry.access_rights & required_access_rights) == required_access_rights;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    enter_method();

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
    dir_entry *source_file = find_dir_entry(source_filename);
    if (source_file == nullptr) {
        std::cout << "File not found" << std::endl;
        return exit_method();
    }
    if (!has_permission(*source_file, READ)) {
        std::cout << "You do not have permission to read " << source_filename << std::endl;
        return exit_method();
    }
    new_file.size = source_file->size;
    new_file.type = source_file->type;
    new_file.access_rights = source_file->access_rights;

    uint8_t buf[source_file->size];
    if (read_data(source_file->first_blk, buf, source_file->size) == -1) {
        exit_method();
        return -1;
    }
    
    int new_blk_no = find_empty_block();
    if (new_blk_no == -1) {
        exit_method();
        return -1;
    }
    new_file.first_blk = new_blk_no;
    
    if (change_cwd(dest_dir) == -1) {
        exit_method();
        return -1;
    }

    if (!has_permission(cwd.info, WRITE)) {
        std::cout << "You do not have permission to create files in this directory." << std::endl;
        return exit_method();
    }
    dir_entry *exists = find_dir_entry(new_file.file_name);
    if (exists != nullptr) {
        std::cout << "File with name " << new_file.file_name << " already exists" << std::endl;
        return exit_method();
    }

    int dir_index = find_empty_dir_index();
    if (dir_index == -1) {
        std::cerr << "No space in directory" << std::endl;
        exit_method();
        return -1;
    }
    if (write_data(new_file.first_blk, std::string((char*)buf)) == -1) {
        exit_method(); 
    };

    cwd.entries[dir_index] = new_file;

    exit_method(true);
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    enter_method();

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

    if (change_cwd(dest_dir) == -1) {
        exit_method();
        return -1;
    };
    if (!has_permission(cwd.info, WRITE | EXECUTE)) {
        std::cout << "You do not have permission to move files to this directory." << std::endl;
        return exit_method();
    }
    dir_entry* exists = find_dir_entry(dest_filename);
    if (exists != nullptr) {
        std::cout << "File with name " << dest_filename << " already exists" << std::endl;
        return exit_method();
    }
    int dir_index = find_empty_dir_index();
    if (dir_index == -1) {
        std::cerr << "No free space in directory\n";
        exit_method();
        return -1;
    }

    if (change_cwd(source_dir) == -1) {
        exit_method();
        return -1;
    }
    if (!has_permission(cwd.info, WRITE | EXECUTE)) {
        std::cout << "You do not have permission to move files from this directory." << std::endl;
        return exit_method();
    }

    dir_entry *file = find_dir_entry(source_filename);
    if (file == nullptr) {
        std::cout << "File not found" << std::endl;
        return exit_method();
    }
    // TODO: Create wipe_file method
    dir_entry file_cp = *file;
    file->first_blk = 0;
    file->size = 0;
    file->type = 0;
    file->access_rights = 0;
    strncpy(file->file_name, "", 56);


    // Update saved cwd
    if (cwd_backup.blk == cwd.blk) {
        memcpy(cwd_backup.entries, cwd.entries, sizeof(cwd.entries));
    }
    if(change_cwd(dest_dir) == -1) {
        exit_method();
        return -1;
    }
    cwd.entries[dir_index] = file_cp;
    strncpy(cwd.entries[dir_index].file_name, dest_filename.c_str(), 56);

    exit_method(true);
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    enter_method();

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
    if (!has_permission(cwd.info, WRITE | EXECUTE)) {
        std::cout << "You do not have permission to remove files in this directory." << std::endl;
        return exit_method();
    }
    dir_entry *file = find_dir_entry(filename);
    if (file == nullptr) {
        std::cout << "File not found" << std::endl;
        return exit_method();
    }
    if (file->type == TYPE_DIR) {
        std::cout << "Can't remove directories" << std::endl;
        return exit_method();
        
    }
    int block_no = file->first_blk;
    file->first_blk = 0;
    file->size = 0;
    memset(file->file_name, 0, 56);


    while (block_no != FAT_EOF) {
        int next_blk = fat[block_no];
        fat[block_no] = FAT_FREE;
        block_no = next_blk;
    }
    exit_method(true);
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    enter_method();

    std::string source_filename, source_dir, dest_filename, dest_dir;
    get_filename_parts(filepath1, &source_filename, &source_dir);
    get_filename_parts(filepath2, &dest_filename, &dest_dir);
    if (source_filename.empty() || dest_filename.empty()) {
        std::cout << "File not found" << std::endl;
        return -1;
    }

    if(change_cwd(source_dir) == -1) {
        return -1;
    }

    dir_entry *source_file = find_dir_entry(source_filename);

    if (change_cwd(dest_dir) == -1) {
        exit_method();
        return -1;
    }

    dir_entry *dest_file = find_dir_entry(dest_filename);

    if (source_file == nullptr || dest_file == nullptr) {
        std::cout << "File not found\n";
        return exit_method();
    }

    if (!has_permission(*source_file, READ)) {
        std::cout << "You do not have permission to read " << source_filename << std::endl;
        return exit_method();
    }
    if (!has_permission(*dest_file, READ | WRITE)) {
        std::cout << "You do not have permission to read or write " << dest_filename << std::endl;
        return exit_method();
    }
    
    int dest_file_last_blk_size = dest_file->size % BLOCK_SIZE;
    int new_size = source_file->size + dest_file->size;
    int buffer_size = source_file->size + dest_file_last_blk_size - 1; // File one + last block of file two without null terminator
    dest_file->size = new_size - 1;  // Remove dest_file null terminator
    
    int dest_file_last_blk = dest_file->first_blk;
    while (fat[dest_file_last_blk] != FAT_EOF) {
        dest_file_last_blk = fat[dest_file_last_blk];
    }

    uint8_t buf[buffer_size]; 
    if (read_data(dest_file_last_blk, buf, dest_file_last_blk_size - 1) == -1) {
        exit_method();
        return -1;
    }
    if (read_data(source_file->first_blk, &buf[dest_file_last_blk_size - 1], source_file->size) == -1) {
        exit_method();
        return -1;
    };

    if (write_data(dest_file_last_blk, std::string((char*)buf)) == -1) {
        exit_method();
        return -1;
    };

    exit_method(true);
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    enter_method();
    
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
    if (!has_permission(cwd.info, WRITE | EXECUTE)) {
        std::cout << "You do not have permission to create directories in this directory." << std::endl;
        return exit_method();
    }
    dir_entry* entry = find_dir_entry(dirname);
    if (entry != nullptr) {
        std::cout << "A file or directory named " << dirname << " already exists in this directory." << std::endl;
        return exit_method();
    }

    dir_entry new_entry;
    strncpy(new_entry.file_name, dirname.c_str(), 56);
    new_entry.size = 0;
    new_entry.first_blk = find_empty_block();
    if (new_entry.first_blk == -1) {
        exit_method();
        return -1;
    }
    new_entry.type = TYPE_DIR;
    new_entry.access_rights = READ | WRITE | EXECUTE;
    int dir_index = find_empty_dir_index();
    if (dir_index == -1) {
        std::cerr << "No free space in directory\n";
        exit_method();
        return -1;
    }
    
    dir_entry new_dir[DIR_SIZE];
    init_dir(new_dir, cwd.blk, cwd.info.access_rights);
    if (disk.write(new_entry.first_blk, (uint8_t*) new_dir) == -1) {
        exit_method();
        return -1;
    };

    cwd.entries[dir_index] = new_entry;
    fat[new_entry.first_blk] = FAT_EOF;

    exit_method(true);
    return 0;   
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    enter_method();
    if (change_cwd(dirpath) == -1) {
        return -1;
    }
    if (!has_permission(cwd.info, READ)) {
        std::cout << "Permisssion denied" << std::endl;
        return exit_method();
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
    if (cwd.blk == ROOT_BLOCK) {
        return "/";
    }
    
    struct dir_entry parent_dir[DIR_SIZE];
    std::stack<std::string> dirs;
    struct dir_entry current_dir;
    int current_blk = cwd.blk;
    int parent_blk = cwd.entries[PARENT_DIR_ENTRY_INDEX].first_blk;

    while (current_blk != ROOT_BLOCK) {
        disk.read(parent_blk, (uint8_t*)parent_dir);
        for(int i = 0; i < DIR_SIZE; i++) {
            if (parent_dir[i].first_blk == current_blk) {
                dirs.push(parent_dir[i].file_name);
                break;
            }
        }
        current_blk = parent_blk;
        parent_blk = parent_dir[PARENT_DIR_ENTRY_INDEX].first_blk;
    }

    std::string res = "";
    while(!dirs.empty()) {
        res.append("/" + dirs.top());
        dirs.pop();
    }
    return res;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    enter_method();

    std::string filename;
    std::string dirname;
    int access_level;
    try {
        access_level = stoi(accessrights);
    }
    catch (std::invalid_argument) {
        std::cout << "Invalid value for chmod" << std::endl;
        return 0;
    }
    if (access_level < 0 || (READ | WRITE | EXECUTE) < access_level) {
        std::cout << "Invalid value for chmod" << std::endl;
        return 0;
    }

    get_filename_parts(filepath, &filename, &dirname);
    if (filename.empty()) {
        filename = dirname.substr(dirname.find_last_of("/") + 1);
        dirname = dirname.substr(0, dirname.find_last_of("/") + 1);
    }

    change_cwd(dirname);
    if (filename.empty() && cwd.blk == ROOT_BLOCK) { // Special case "/"
        filename = "..";
    }
    dir_entry* entry = find_dir_entry(filename);
    if (entry == nullptr) {
        std::cout << "File or directory " << filename << " not found" << std::endl;
        return exit_method();
    }
    entry->access_rights = access_level;
    if (cwd.blk == entry->first_blk) { // Changing own permission
        cwd.info.access_rights = access_level;
    }

    // Change all references to changed dir
    if (entry->type == TYPE_DIR){
        struct dir_entry changed_dir[DIR_SIZE];
        if (entry->first_blk == cwd.blk) {
            memcpy(changed_dir, cwd.entries, sizeof(cwd.entries));
        } else {
            disk.read(entry->first_blk, (uint8_t*)changed_dir);
        }
        for (int i = 1; i < DIR_SIZE; i++) {
            if (changed_dir[i].type == TYPE_DIR) {
                struct dir_entry child_dir[DIR_SIZE];
                disk.read(changed_dir[i].first_blk, (uint8_t*)child_dir);
                
                child_dir[PARENT_DIR_ENTRY_INDEX].access_rights = access_level;
                
                disk.write(changed_dir[i].first_blk, (uint8_t*)child_dir);
                
                if (changed_dir[i].first_blk = cwd_backup.blk) {
                    cwd_backup.entries[PARENT_DIR_ENTRY_INDEX].access_rights = access_level;
                }
            }
        }
    }
    exit_method(true);
    return 0;
}
