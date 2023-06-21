#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

// ==================== VAR DEF ====================

static char block_copy[NEW_BLOCK_SIZE];
static char block_copy_copy[NEW_BLOCK_SIZE];

// Super Block Structure / Copy
static char super_block_copy[NEW_BLOCK_SIZE];
static super_block_structure *created_super_block = (super_block_structure *)super_block_copy;

// Pointers for last bitmap pos
static uint16_t inode_bitmap_last = 0;
static uint16_t dblock_bitmap_last = 0;

// Pwd
static uint16_t pwd;

// File Descriptor
static file_desc_structure file_desc_table[MAX_FILE_OPEN];

// Bitmap
static char inode_bitmap_block_copy[NEW_BLOCK_SIZE];
static char dblock_bitmap_block_copy[NEW_BLOCK_SIZE];

// ======================================== COMMANDS ========================================

#include "fsutil.c"

void fs_init(void)
{
    block_init(); // Call block init

    // Pointer to copy based on SB structre
    created_super_block = (super_block_structure *)super_block_copy;

    adapt_block_read(SUPER_BLOCK, super_block_copy);

    // Verify magic number
    if (created_super_block->magic_num != MAGIC_NUMBER)
    {
        // Try backup
        adapt_block_read(SUPER_BLOCK_BACKUP, super_block_copy);
        if (created_super_block->magic_num != MAGIC_NUMBER)
        {
            fs_mkfs(); // Disk formating
            return;
        }
        else
            adapt_block_write(SUPER_BLOCK, super_block_copy);
    }

    // Root directory stored at pwd var
    pwd = (uint16_t)PWD_ID_ROOT_DIR;

    // Bzero to clear file descriptor table
    bzero((char *)file_desc_table, sizeof(file_desc_table));

    //  Load bitmaps
    adapt_block_read(created_super_block->inode_bitmap_place, inode_bitmap_block_copy);
    adapt_block_read(created_super_block->dblock_bitmap_place, dblock_bitmap_block_copy);
}

int fs_mkfs(void)
{
    // Init super block with structure
    created_super_block = (super_block_structure *)super_block_copy;

    created_super_block->file_sys_size = FS_SIZE;
    created_super_block->inode_count = 1; // Initial inode number
    created_super_block->inode_bitmap_place = SUPER_BLOCK + 1;
    created_super_block->inode_start = SUPER_BLOCK + 3;
    created_super_block->magic_num = MAGIC_NUMBER;
    created_super_block->dblock_bitmap_place = SUPER_BLOCK + 2;
    created_super_block->dblock_start = SUPER_BLOCK + 3 + INODE_BLOCK_NUMBER;

    // Create Backup
    adapt_block_write(SUPER_BLOCK, super_block_copy);
    adapt_block_write(SUPER_BLOCK_BACKUP, super_block_copy);

    // Reset bitmap for inode and data block
    bzero_block_custom(created_super_block->inode_bitmap_place);
    bzero_block_custom(created_super_block->dblock_bitmap_place);

    bzero(inode_bitmap_block_copy, NEW_BLOCK_SIZE);
    bzero(dblock_bitmap_block_copy, NEW_BLOCK_SIZE);

    // Reset pointers
    inode_bitmap_last = 0;
    dblock_bitmap_last = 0;

    inode temp_root;
    inode_init(&temp_root, MY_DIRECTORY);
    inode_write(PWD_ID_ROOT_DIR, &temp_root);
    write_bitmap_block(INODE_BITMAP, PWD_ID_ROOT_DIR, 1);

    // Adding directory entries to root "." and ".."
    int res;
    res = dir_entry_add(PWD_ID_ROOT_DIR, PWD_ID_ROOT_DIR, ".");
    if (res < 0)
    {
        bzero(super_block_copy, NEW_BLOCK_SIZE);
        sb_write();
        return -1;
    }
    res = dir_entry_add(PWD_ID_ROOT_DIR, PWD_ID_ROOT_DIR, "..");
    if (res < 0)
    {
        bzero(super_block_copy, NEW_BLOCK_SIZE);
        sb_write();
        return -1;
    }

    // Mount at root directory
    pwd = PWD_ID_ROOT_DIR;

    // Clear table
    bzero((char *)file_desc_table, sizeof(file_desc_table));

    return 0;
}

int fs_open(char *fileName, int flags)
{
    return -1;
}

int fs_close(int fd)
{
    return -1;
}

int fs_read(int fd, char *buf, int count)
{
    return -1;
}

int fs_write(int fd, char *buf, int count)
{
    return -1;
}

int fs_lseek(int fd, int offset)
{
    return -1;
}

int fs_mkdir(char *fileName)
{
    if (strlen(fileName) > MAX_FILE_NAME)
    {
        ERROR_MSG(("Too long file name!\n"))
        return -1;
    }

    // TODO: Check same file name
    // if (dir_entry_find(pwd, fileName) >= 0)
    // {
    //     ERROR_MSG(("Already have a same name file !\n"))
    //     return -1;
    // }

    int new_inode = inode_create(MY_DIRECTORY);
    if (new_inode < 0)
        return -1;
    if (dir_entry_add(new_inode, new_inode, ".") < 0)
    {
        inode_free(new_inode);
        return -1;
    }
    if (dir_entry_add(new_inode, pwd, "..") < 0)
    {
        inode_free(new_inode);
        return -1;
    }
    if (dir_entry_add(pwd, new_inode, fileName) < 0)
    {
        inode_free(new_inode);
        return -1;
    }
    return new_inode;
}

int fs_rmdir(char *fileName)
{
    return -1;
}

int fs_cd(char *dirName)
{
    int path_res = path_resolve(dirName, pwd, MY_DIRECTORY);
    if (path_res < 0)
        return -1;
    inode temp_inode;
    inode_read(path_res, &temp_inode);
    if (temp_inode.type != MY_DIRECTORY)
    {
        ERROR_MSG(("%s is not a dir\n", dirName));
        return -1;
    }
    pwd = path_res;
    return 0;
}

int fs_link(char *old_fileName, char *new_fileName)
{
    return -1;
}

int fs_unlink(char *fileName)
{
    return -1;
}

int fs_stat(char *fileName, fileStat *buf)
{
    return -1;
}

int fs_ls()
{
    inode dir_inode;
    dir_entry dir_entries[NEW_BLOCK_SIZE / sizeof(dir_entry)];

    inode_read(pwd, &dir_inode);

    int i, j;

    // printf(".\n");
    // printf("..\n");

    for (i = 0; i < DIRECT_BLOCK; i++)
    {
        if (dir_inode.blocks[i] != 0)
        {
            dblock_read(dir_inode.blocks[i], (char *)dir_entries);

            for (j = 0; j < NEW_BLOCK_SIZE / sizeof(dir_entry); j++)
            {
                if (dir_entries[j].inode_id != 0)
                {
                    printf("%s\n", dir_entries[j].file_name);
                }
            }
        }
    }
    return 0;
}
