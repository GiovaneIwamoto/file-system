#ifndef FS_INCLUDED
#define FS_INCLUDED

// ------------------------------ FILE SYSTEM ------------------------------

#define FS_SIZE 2048

void fs_init(void);
int fs_mkfs(void);
int fs_open(char *fileName, int flags);
int fs_close(int fd);
int fs_read(int fd, char *buf, int count);
int fs_write(int fd, char *buf, int count);
int fs_lseek(int fd, int offset);
int fs_mkdir(char *fileName);
int fs_rmdir(char *fileName);
int fs_cd(char *dirName);
int fs_link(char *old_fileName, char *new_fileName);
int fs_unlink(char *fileName);
int fs_stat(char *fileName, fileStat *buf);

#define MAX_FILE_NAME 32

#define MAX_PATH_NAME 256 // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#define MAX_FILE_OPEN 256

#define NEW_BLOCK_SIZE 4096
#define MAX_FILE_COUNT (2048)

// ------------------------------ SUPER BLOCK ------------------------------

#define SUPER_BLOCK 1 // Super Block number

#define SUPER_BLOCK_BACKUP (FS_SIZE / 8 - 1)

// Number of blocks reserved for storing inodes -> 16
#define INODE_BLOCK_NUMBER (MAX_FILE_COUNT / INODE_PER_BLOCK)

// Number of blocks available for storing data in the file system -> 235
#define DATA_BLOCK_NUMBER (FS_SIZE / 8 - 5 - INODE_BLOCK_NUMBER)

// TODO: Checkout this padding
//  Padding size required in the super block structure
#define SB_PADDING (NEW_BLOCK_SIZE - 18)

// Magic number
#define MAGIC_NUMBER 01234567

typedef struct __attribute__((__packed__))
{
    uint16_t file_sys_size;
    uint16_t inode_bitmap_place;
    uint16_t inode_start;
    uint16_t inode_count;
    uint32_t magic_num;
    uint16_t dblock_bitmap_place;
    uint16_t dblock_start;
    uint16_t dblock_count;

    char _padding[SB_PADDING];

} super_block_structure;

// ---------- INODE ------------------------------

#define INODE_PER_BLOCK (NEW_BLOCK_SIZE / 32)

// ---------- FILE DESCRIPTOR ------------------------------

typedef struct
{
    bool_t is_using; // Boolean for active or inactive
    uint32_t cursor; // File position
    uint16_t inode_id;
    uint16_t mode;

} file_desc_structure;

// ---------- DIRECTORY ------------------------------

#define PWD_ID_ROOT_DIR 0

// ------------------------------------------------------------

#endif

/*
FILE SYSTEM = 1MB = 1.048.576 BYTES
BLOCK SIZE = 512 BYTES

BLOCKS = 2048 BLOCKS
INODES = 16 BLOCKS
DATA = 235 BLOCKS
*/
