#include <iostream>
#include <cstdint>
#include <stack>
#include "disk.h"

#ifndef __FS_H__
#define __FS_H__

#define ROOT_BLOCK 0
#define FAT_BLOCK 1
#define FAT_FREE 0
#define FAT_EOF -1
#define PARENT_DIR_ENTRY_INDEX 0

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0x04
#define WRITE 0x02
#define EXECUTE 0x01

#define DIR_SIZE BLOCK_SIZE/sizeof(dir_entry)
#define FAT_ENTRIES BLOCK_SIZE/2

struct dir_entry {
    char file_name[56]; // name of the file / sub-directory
    uint32_t size; // size of the file in bytes
    uint16_t first_blk; // index in the FAT for the first block of the file or the directory block for directories
    uint8_t type; // directory (1) or file (0)
    uint8_t access_rights; // read (0x04), write (0x02), execute (0x01)
};

struct cwd_struct {
    dir_entry entries[DIR_SIZE];
    dir_entry info;
    int blk;
};

class FS {
private:
    Disk disk;
    // size of a FAT entry is 2 bytes
    int16_t fat[BLOCK_SIZE/2];
    struct dir_entry root_dir[DIR_SIZE];
    cwd_struct cwd;
    cwd_struct cwd_backup;
    // struct dir_entry cwd[DIR_SIZE];
    // struct dir_entry cwd_info;
    // int cwd_blk;

public:
    FS();
    ~FS();

    int find_empty_block();
    int write_data(int starting_block, std::string data);
    int read_data(int start_blk, uint8_t* out_buf, size_t size);
    int init_dir(struct dir_entry *dir, int parent_blk, uint8_t access_rights);
    std::string get_pwd_string();
    dir_entry* find_dir_entry(std::string filename);
    int find_empty_dir_index();

    void get_filename_parts(std::string filepath, std::string *filename, std::string *dirpath);
    int find_dir_from_path(std::string dirpath);
    int change_cwd(std::string dirpath);
    int exit_method(dir_entry cur_cwd[BLOCK_SIZE/sizeof(dir_entry)], int cwd_blk, dir_entry cur_cwd_info, int save);
    bool dir_entry_is_empty(dir_entry entry);
    bool has_permission(dir_entry entry, uint8_t required_access_rights);
    int enter_method();
    int exit_method(bool save);

    // formats the disk, i.e., creates an empty file system
    int format();
    // create <filepath> creates a new file on the disk, the data content is
    // written on the following rows (ended with an empty row)
    int create(std::string filepath);
    // cat <filepath> reads the content of a file and prints it on the screen
    int cat(std::string filepath);
    // ls lists the content in the currect directory (files and sub-directories)
    int ls();

    // cp <sourcepath> <destpath> makes an exact copy of the file
    // <sourcepath> to a new file <destpath>
    int cp(std::string sourcepath, std::string destpath);
    // mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
    // or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
    int mv(std::string sourcepath, std::string destpath);
    // rm <filepath> removes / deletes the file <filepath>
    int rm(std::string filepath);
    // append <filepath1> <filepath2> appends the contents of file <filepath1> to
    // the end of file <filepath2>. The file <filepath1> is unchanged.
    int append(std::string filepath1, std::string filepath2);

    // mkdir <dirpath> creates a new sub-directory with the name <dirpath>
    // in the current directory
    int mkdir(std::string dirpath);
    // cd <dirpath> changes the current (working) directory to the directory named <dirpath>
    int cd(std::string dirpath);
    // pwd prints the full path, i.e., from the root directory, to the current
    // directory, including the currect directory name
    int pwd();

    // chmod <accessrights> <filepath> changes the access rights for the
    // file <filepath> to <accessrights>.
    int chmod(std::string accessrights, std::string filepath);
};

#endif // __FS_H__
