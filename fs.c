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

#include "fsutil.c"

// ==================== INIT ====================

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

// ==================== MKFS ====================

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

// ==================== OPEN ====================

int fs_open(char *fileName, int flags)
{
    int resolved_path = path_resolve(fileName, pwd, MY_DIRECTORY);

    // Flag validation
    if (flags != FS_O_RDONLY && flags != FS_O_WRONLY && flags != FS_O_RDWR)
        return -1;

    int new_fd = fd_open(resolved_path, flags);
    if (new_fd < 0) // Invalid file
        return -1;

    if (resolved_path < 0) // No file
    {
        if (flags == FS_O_RDONLY) // Flag set to read only, cannot open for write
        {
            fd_close(new_fd); // Close file descriptor
            ERROR_MSG(("%s Can not open besides read only.\n", fileName))
            return -1;
        }
        else // Not only read only case
        {
            resolved_path = path_resolve(fileName, pwd, 2);
            if (resolved_path < 0)
            {
                fd_close(new_fd);
                ERROR_MSG(("Path does not exist.\n"));
                return -1;
            }
        }
    }
    else
    {
        inode temporary;
        inode_read(resolved_path, &temporary); // Read inode info
        if (flags != FS_O_RDONLY && temporary.type == MY_DIRECTORY)
        {
            ERROR_MSG(("%s is a dir.\n", fileName))
            fd_close(new_fd);
            return -1;
        }
        file_desc_table[new_fd].inode_id = resolved_path; // Store at descriptor table
    }
    return new_fd;
}

// ==================== CLOSE ====================

int fs_close(int fd)
{
    if (fd < 0 || fd >= MAX_FILE_OPEN)
    {
        ERROR_MSG(("Wrong fd input!\n"))
        return -1;
    }
    if (file_desc_table[fd].is_using == FALSE)
    {
        ERROR_MSG(("fd %d is not using!", fd))
        return -1;
    }
    fd_close(fd);

    // check whether we need to delete the file
    if (fd_find_same_num(file_desc_table[fd].inode_id) == 0)
    {
        inode temp;
        inode_read(file_desc_table[fd].inode_id, &temp);
        if (temp.link_count == 0) // need to free the file
            inode_free(file_desc_table[fd].inode_id);
    }
    return fd;
}

// ==================== READ ====================

int fs_read(int fd, char *buf, int count)
{
    if (count < 0)
    {
        ERROR_MSG(("Wrong count input!\n"))
        return -1;
    }
    if (fd < 0 || fd >= MAX_FILE_OPEN)
    {
        ERROR_MSG(("Wrong fd input!\n"))
        return -1;
    }
    if (file_desc_table[fd].is_using == FALSE)
    {
        ERROR_MSG(("fd %d is not using!", fd))
        return -1;
    }
    if (file_desc_table[fd].mode == FS_O_WRONLY)
    {
        ERROR_MSG(("can't read the file open as write-only file"))
        return -1;
    }
    if (count == 0)
    {
        return 0;
    }
    inode temp_file;
    inode_read(file_desc_table[fd].inode_id, &temp_file);

    int real_count = 0;
    if (file_desc_table[fd].cursor >= temp_file.size)
        return 0;
    if (file_desc_table[fd].cursor + count > temp_file.size)
        count = temp_file.size - file_desc_table[fd].cursor;

    int end_block = (file_desc_table[fd].cursor + count - 1) / NEW_BLOCK_SIZE;
    int in_end_block_cursor = (file_desc_table[fd].cursor + count - 1) % NEW_BLOCK_SIZE;

    while (real_count < count)
    {
        int now_block = file_desc_table[fd].cursor / NEW_BLOCK_SIZE;
        int now_block_id;
        if (now_block >= DIRECT_BLOCK)
        {
            dblock_read(temp_file.blocks[DIRECT_BLOCK], block_copy);
            uint16_t *block_list = (uint16_t *)block_copy;
            now_block_id = block_list[now_block - DIRECT_BLOCK];
        }
        else
        {
            now_block_id = temp_file.blocks[now_block];
        }
        dblock_read(now_block_id, block_copy);

        int rdy_count;
        if (now_block < end_block)
            rdy_count = NEW_BLOCK_SIZE - file_desc_table[fd].cursor % NEW_BLOCK_SIZE;
        else
            rdy_count = in_end_block_cursor - file_desc_table[fd].cursor % NEW_BLOCK_SIZE + 1;
        bcopy((unsigned char *)(block_copy + file_desc_table[fd].cursor % NEW_BLOCK_SIZE), (unsigned char *)buf, rdy_count);
        buf += rdy_count;
        real_count += rdy_count;
        file_desc_table[fd].cursor += rdy_count;
    }
    return real_count;
}

// ==================== WRITE ====================

int fs_write(int fd, char *buf, int count)
{
    if (count < 0)
    {
        ERROR_MSG(("Wrong count input!\n"))
        return -1;
    }
    if (fd < 0 || fd >= MAX_FILE_OPEN)
    {
        ERROR_MSG(("Wrong fd input!\n"))
        return -1;
    }
    if (file_desc_table[fd].is_using == FALSE)
    {
        ERROR_MSG(("fd %d is not using!", fd))
        return -1;
    }
    if (file_desc_table[fd].mode == FS_O_RDONLY)
    {
        ERROR_MSG(("can't write the file open as read-only file"))
        return -1;
    }
    if (count == 0)
    {
        return 0;
    }
    inode temp_file;
    inode_read(file_desc_table[fd].inode_id, &temp_file);
    int temp_size = temp_file.size;

    int total_block_num = (temp_file.size - 1 + NEW_BLOCK_SIZE) / NEW_BLOCK_SIZE;
    int end_block_num = (file_desc_table[fd].cursor + count - 1 + NEW_BLOCK_SIZE) / NEW_BLOCK_SIZE;
    int in_end_block_cursor = (file_desc_table[fd].cursor + count - 1) % NEW_BLOCK_SIZE;
    while (total_block_num < end_block_num)
    {
        if (alloc_dblock_mount_to_inode(file_desc_table[fd].inode_id) < 0)
        {
            end_block_num = total_block_num;
            in_end_block_cursor = NEW_BLOCK_SIZE - 1;
            break;
        }
        inode_read(file_desc_table[fd].inode_id, &temp_file);
        temp_file.size += NEW_BLOCK_SIZE;
        inode_write(file_desc_table[fd].inode_id, &temp_file);
        total_block_num++;
    }

    count = (end_block_num - 1) * NEW_BLOCK_SIZE + in_end_block_cursor - file_desc_table[fd].cursor + 1;

    if (file_desc_table[fd].cursor + count > temp_size)
        temp_file.size = file_desc_table[fd].cursor + count;
    else
        temp_file.size = temp_size;

    inode_write(file_desc_table[fd].inode_id, &temp_file);

    int real_count = 0;
    while (real_count < count)
    {
        int now_block = file_desc_table[fd].cursor / NEW_BLOCK_SIZE;
        int now_block_id;
        if (now_block >= DIRECT_BLOCK)
        {
            dblock_read(temp_file.blocks[DIRECT_BLOCK], block_copy);
            uint16_t *block_list = (uint16_t *)block_copy;
            now_block_id = block_list[now_block - DIRECT_BLOCK];
        }
        else
        {
            now_block_id = temp_file.blocks[now_block];
        }
        dblock_read(now_block_id, block_copy);

        int rdy_count;
        if (now_block < end_block_num - 1)
            rdy_count = NEW_BLOCK_SIZE - file_desc_table[fd].cursor % NEW_BLOCK_SIZE;
        else
            rdy_count = in_end_block_cursor - file_desc_table[fd].cursor % NEW_BLOCK_SIZE + 1;
        bcopy((unsigned char *)buf, (unsigned char *)(block_copy + file_desc_table[fd].cursor % NEW_BLOCK_SIZE), rdy_count);
        dblock_write(now_block_id, block_copy);
        buf += rdy_count;
        real_count += rdy_count;
        file_desc_table[fd].cursor += rdy_count;
    }
    return real_count;
}

int fs_lseek(int fd, int offset)
{
    return -1;
}

// ==================== MKDIR ====================

int fs_mkdir(char *fileName)
{
    if (strlen(fileName) > MAX_FILE_NAME)
    {
        ERROR_MSG(("Too long file name!\n"))
        return -1;
    }

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

// ==================== INIT ====================

int fs_rmdir(char *fileName)
{
    return -1;
}

// ==================== CD ====================

int fs_cd(char *dirName)
{
    int resolved_path = path_resolve(dirName, pwd, MY_DIRECTORY);
    if (resolved_path < 0)
        return -1;
    inode temp_inode;
    inode_read(resolved_path, &temp_inode);
    if (temp_inode.type != MY_DIRECTORY)
    {
        ERROR_MSG(("%s is not a dir\n", dirName));
        return -1;
    }
    pwd = resolved_path;
    return 0;
}

// ==================== TODO: ====================

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

// ==================== LS ====================

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
