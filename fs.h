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
#define MAX_PATH_NAME 256

#define MAX_FILE_OPEN 256

#define NEW_BLOCK_SIZE 4096
#define MAX_FILE_COUNT (2048)

#if (NEW_BLOCK_SIZE * DIRECT_BLOCK + NEW_BLOCK_SIZE * NEW_BLOCK_SIZE / 2) < (DATA_BLOCK_NUMBER * NEW_BLOCK_SIZE)
#define MAX_FILE_SIZE (NEW_BLOCK_SIZE * DIRECT_BLOCK + NEW_BLOCK_SIZE * NEW_BLOCK_SIZE / 2)
#else
#define MAX_FILE_SIZE (DATA_BLOCK_NUMBER * NEW_BLOCK_SIZE)
#endif

#define MAX_FILE_ONE_DIR (DIR_ENTRY_PER_BLOCK * DIRECT_BLOCK + DIR_ENTRY_PER_BLOCK * NEW_BLOCK_SIZE / 2)

// Bitmap , Dir
#define INODE_BITMAP 0
#define DBLOCK_BITMAP 1

#define MY_DIRECTORY 0
#define REAL_FILE 1

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

#define DIRECT_BLOCK 11
#define INODE_PADDING 0

#define INODE_PER_BLOCK (NEW_BLOCK_SIZE / 32)

#define MAX_BLOCKS_INDEX_IN_INODE (DIRECT_BLOCK + NEW_BLOCK_SIZE / 2)

typedef struct __attribute__((__packed__))
{
    uint32_t size; // in bytes
    uint16_t type; // 0 for dir, 1 for file
    uint16_t link_count;
    uint16_t blocks[DIRECT_BLOCK + 1]; // start from 0 as data block index
} inode;

// ---------- DIRECTORY ENTRY ------------------------------

#define PWD_ID_ROOT_DIR 0

#define DIR_ENTRY_PADDING (64 - 2 - MAX_FILE_NAME - 1)
#define DIR_ENTRY_PER_BLOCK (NEW_BLOCK_SIZE / 64)

typedef struct __attribute__((__packed__))
{
    uint16_t inode_id;
    char file_name[MAX_FILE_NAME + 1];
    char _padding[DIR_ENTRY_PADDING];

} dir_entry;

// ---------- FILE DESCRIPTOR ------------------------------

typedef struct
{
    bool_t is_using; // Boolean for active or inactive
    uint32_t cursor; // File position
    uint16_t inode_id;
    uint16_t mode;

} file_desc_structure;

// ------------------------------------------------------------

#endif

/*
FILE SYSTEM = 1MB = 1.048.576 BYTES
BLOCK SIZE = 512 BYTES

BLOCKS = 2048 BLOCKS
INODES = 16 BLOCKS
DATA = 235 BLOCKS
*/
