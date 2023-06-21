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
    inode_init(&temp_root, POS_DIRECTORY);
    inode_write(PWD_ID_ROOT_DIR, &temp_root);
    write_bitmap_block(INODE_BITMAP, PWD_ID_ROOT_DIR, 1);

    // Adding directory entries to root "." and ".."
    int res;
    res = add_2_directory_entry(PWD_ID_ROOT_DIR, PWD_ID_ROOT_DIR, ".");
    if (res < 0)
    {
        bzero(super_block_copy, NEW_BLOCK_SIZE);
        sb_write();
        return -1;
    }
    res = add_2_directory_entry(PWD_ID_ROOT_DIR, PWD_ID_ROOT_DIR, "..");
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
    int resolved_path = path_resolve(fileName, pwd, POS_DIRECTORY);

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
        if (flags != FS_O_RDONLY && temporary.type == POS_DIRECTORY)
        {
            ERROR_MSG(("%s is a dir.\n", fileName))
            fd_close(new_fd); // Close return error
            return -1;
        }
        file_desc_table[new_fd].inode_id = resolved_path; // Store at descriptor table
    }
    return new_fd;
}

// ==================== CLOSE ====================

int fs_close(int fd)
{
    // Check file descriptor usage
    if (file_desc_table[fd].is_using == FALSE)
    {
        ERROR_MSG(("%d this file descriptor is not in use.", fd))
        return -1;
    }

    if (fd < 0 || fd >= MAX_FILE_OPEN)
    {
        ERROR_MSG(("Wrong input for file descriptor.\n"))
        return -1;
    }
    fd_close(fd);

    // Opened file descriptors that shares same inode id
    if (fd_find_same_num(file_desc_table[fd].inode_id) == 0)
    {
        inode temp;
        // Read inode infos
        inode_read(file_desc_table[fd].inode_id, &temp);
        if (temp.link_count == 0)
            inode_free(file_desc_table[fd].inode_id);
    }
    return fd;
}

// ==================== READ ====================

int fs_read(int fd, char *buf, int count)
{
    // Nothing to read bytes case
    if (count == 0)
    {
        return 0;
    }

    // Temporary file with defined inode structure
    inode temporary_file;
    inode_read(file_desc_table[fd].inode_id, &temporary_file);

    int byte_read = 0;
    if (file_desc_table[fd].cursor >= temporary_file.size)
        return 0;

    // Reading limiter
    if (file_desc_table[fd].cursor + count > temporary_file.size)
        count = temporary_file.size - file_desc_table[fd].cursor;

    int final_block = (file_desc_table[fd].cursor + count - 1) / NEW_BLOCK_SIZE;
    int cursor_4_final_block = (file_desc_table[fd].cursor + count - 1) % NEW_BLOCK_SIZE;

    while (byte_read < count) // Read blocks as necessary to fill buffer
    {
        int block_live = file_desc_table[fd].cursor / NEW_BLOCK_SIZE;
        int block_live_id;
        if (block_live >= DIRECT_BLOCK) // Direct or Id index block
        {
            dblock_read(temporary_file.blocks[DIRECT_BLOCK], block_copy);

            uint16_t *block_list = (uint16_t *)block_copy;
            block_live_id = block_list[block_live - DIRECT_BLOCK];
        }
        else
        {
            block_live_id = temporary_file.blocks[block_live];
        }
        dblock_read(block_live_id, block_copy);

        int rdy_count; // Copying bytes block to buff

        if (block_live < final_block)
            rdy_count = NEW_BLOCK_SIZE - file_desc_table[fd].cursor % NEW_BLOCK_SIZE;
        else
            rdy_count = cursor_4_final_block - file_desc_table[fd].cursor % NEW_BLOCK_SIZE + 1;

        bcopy((unsigned char *)(block_copy + file_desc_table[fd].cursor % NEW_BLOCK_SIZE), (unsigned char *)buf, rdy_count);
        buf += rdy_count;
        byte_read += rdy_count;
        file_desc_table[fd].cursor += rdy_count;
    }
    return byte_read;
}

// ==================== WRITE ====================

// writes count bytes to the file referenced by the file descriptor fd of buffer indicated by buf
int fs_write(int fd, char *buf, int count)
{
    if (count == 0) // Return 0 without any effect
        return 0;

    // Error cases:
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

    inode temporary_file_base; // Represent metadata file
    inode_read(file_desc_table[fd].inode_id, &temporary_file_base);
    int temp_size = temporary_file_base.size;

    // Block storage space
    int end_block_num = (file_desc_table[fd].cursor + count - 1 + NEW_BLOCK_SIZE) / NEW_BLOCK_SIZE;
    int in_end_block_cursor = (file_desc_table[fd].cursor + count - 1) % NEW_BLOCK_SIZE;

    count = (end_block_num - 1) * NEW_BLOCK_SIZE + in_end_block_cursor - file_desc_table[fd].cursor + 1;

    if (file_desc_table[fd].cursor + count > temp_size)
        temporary_file_base.size = file_desc_table[fd].cursor + count;
    else
        temporary_file_base.size = temp_size;

    inode_write(file_desc_table[fd].inode_id, &temporary_file_base); // Write back metadata

    // Byte writting
    int byte_counter = 0;
    while (byte_counter < count)
    {
        int now_block = file_desc_table[fd].cursor / NEW_BLOCK_SIZE;
        int now_block_id;

        if (now_block >= DIRECT_BLOCK)
        {
            dblock_read(temporary_file_base.blocks[DIRECT_BLOCK], block_copy);
            uint16_t *block_list = (uint16_t *)block_copy;
            now_block_id = block_list[now_block - DIRECT_BLOCK];
        }

        else
        {
            now_block_id = temporary_file_base.blocks[now_block];
        }
        dblock_read(now_block_id, block_copy);

        int to_be_written;

        if (now_block < end_block_num - 1)
            to_be_written = NEW_BLOCK_SIZE - file_desc_table[fd].cursor % NEW_BLOCK_SIZE;

        else
            to_be_written = in_end_block_cursor - file_desc_table[fd].cursor % NEW_BLOCK_SIZE + 1;

        bcopy((unsigned char *)buf, (unsigned char *)(block_copy + file_desc_table[fd].cursor % NEW_BLOCK_SIZE), to_be_written);
        dblock_write(now_block_id, block_copy);

        buf = buf + to_be_written;

        byte_counter = byte_counter + to_be_written;
        file_desc_table[fd].cursor += to_be_written;
    }
    return byte_counter;
}

// ==================== SEEK ====================

int fs_lseek(int fd, int offset)
{
    return -1;
}

// ==================== MKDIR ====================

int fs_mkdir(char *fileName)
{
    // Same file name case
    if (dir_entry_find(pwd, fileName) >= 0)
    {
        ERROR_MSG(("Name already in use.\n"))
        return -1;
    }

    if (strlen(fileName) > MAX_FILE_NAME) // File name limiter
    {
        ERROR_MSG(("File name size beyond limit.\n"))
        return -1;
    }

    // New inode for current directory
    int created_inode = inode_create(POS_DIRECTORY);
    if (created_inode < 0)
        return -1;

    // Inode entrys for current and parent representations
    if (add_2_directory_entry(created_inode, created_inode, ".") < 0)
    {
        inode_free(created_inode);
        return -1;
    }
    if (add_2_directory_entry(created_inode, pwd, "..") < 0)
    {
        inode_free(created_inode);
        return -1;
    }
    if (add_2_directory_entry(pwd, created_inode, fileName) < 0)
    {
        inode_free(created_inode);
        return -1;
    }
    return created_inode;
}

// ==================== RMDIR ====================

int fs_rmdir(char *fileName)
{
    return -1;
}
// ==================== CD ====================

int fs_cd(char *dirName)
{
    // Verify dir path based on pwd
    int determined_path = path_resolve(dirName, pwd, POS_DIRECTORY);
    if (determined_path < 0)
        return -1;

    inode l_inode;
    inode_read(determined_path, &l_inode);

    if (l_inode.type != POS_DIRECTORY) // Not dir case
    {
        ERROR_MSG(("%s is not a dir\n", dirName));
        return -1;
    }
    pwd = determined_path; // Update pwd
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

    for (i = 0; i < DIRECT_BLOCK; i++) // Loop for blocks
    {
        if (dir_inode.blocks[i] != 0)
        {
            dblock_read(dir_inode.blocks[i], (char *)dir_entries); // Read

            for (j = 0; j < NEW_BLOCK_SIZE / sizeof(dir_entry); j++) // Dir entries
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
