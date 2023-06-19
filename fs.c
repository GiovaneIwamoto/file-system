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

// ---------- VAR DEF ----------

static char block_copy[NEW_BLOCK_SIZE];

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

// ==================== STRING COPY LIMIT ====================
// Prevents buffer overflow when copying strings

static void strcpy_limited(char *src, char *dest, int dest_max_len)
{
    // Verify max length
    if (strlen(src) > dest_max_len)
        bcopy((unsigned char *)src, (unsigned char *)dest, dest_max_len);
    else
        bcopy((unsigned char *)src, (unsigned char *)dest, strlen(src));
    dest[dest_max_len] = '\0';
}

// ==================== BLOCK READ WRITE ====================

// Read multiple blocks of data from fs.
static void adapt_block_read(int block, char *mem)
{
    int i;
    for (i = 0; i < NEW_BLOCK_SIZE / BLOCK_SIZE; i++)
    {
        block_read(block * 8 + i, mem + i * BLOCK_SIZE); // Uses block_read
    }
}

// Adapts to the underlying block size by writing the data in chunks of the appropriate size
static void adapt_block_write(int block, char *mem)
{
    int i;
    for (i = 0; i < NEW_BLOCK_SIZE / BLOCK_SIZE; i++)
    {
        block_write(block * 8 + i, mem + i * BLOCK_SIZE);
    }
}

static void sb_write()
{
    adapt_block_write(SUPER_BLOCK, super_block_copy);
    adapt_block_write(SUPER_BLOCK_BACKUP, super_block_copy);
}

// ==================== BITMAP FOR INODE OR DATA ====================

static void write_bitmap_block(int i_d, int index, int val) // 0 for inode bitmap,1 for data bitmap
{
    char *bitmap_block_scratch;
    if (i_d)
        bitmap_block_scratch = dblock_bitmap_block_copy;
    else
        bitmap_block_scratch = inode_bitmap_block_copy;

    int nbyte = index / 8;
    uint8_t the_byte = bitmap_block_scratch[nbyte];
    int mask_off = index % 8;
    uint8_t mask = 1 << mask_off;
    the_byte = the_byte & (~mask);
    if (val)
        the_byte = the_byte | mask;

    bitmap_block_scratch[nbyte] = the_byte;

    if (i_d)
        adapt_block_write(created_super_block->dblock_bitmap_place, bitmap_block_scratch);
    else
        adapt_block_write(created_super_block->inode_bitmap_place, bitmap_block_scratch);
}

static int read_bitmap_block(int i_d, int index) // 0 for inode bitmap,1 for data bitmap
{
    char *bitmap_block_scratch;
    if (i_d)
        bitmap_block_scratch = dblock_bitmap_block_copy;
    else
        bitmap_block_scratch = inode_bitmap_block_copy;
    int nbyte = index / 8;
    uint8_t the_byte = bitmap_block_scratch[nbyte];
    int mask_off = index % 8;
    uint8_t mask = 1 << mask_off;
    return (mask & the_byte) ? 1 : 0;
}

// ==================== DATA BLOCK READ WRITE FREE ====================

static void dblock_read(int index, char *block_buff)
{
    adapt_block_read(created_super_block->dblock_start + index, block_buff);
}

static void dblock_write(int index, char *block_buff)
{
    adapt_block_write(created_super_block->dblock_start + index, block_buff);
}

static void dblock_free(int index)
{
    int temp = read_bitmap_block(DBLOCK_BITMAP, index);
    if (temp)
    {
        created_super_block->dblock_count--;
        sb_write();
    }
    write_bitmap_block(DBLOCK_BITMAP, index, 0);
}

// =================================
static int find_next_free(int i_d) // must alloc(write 1) after this function find the result
{
    int i;
    int res;
    if (i_d)
    {
        i = (dblock_bitmap_last + 1) % DATA_BLOCK_NUMBER;
        while (i != dblock_bitmap_last)
        {
            res = read_bitmap_block(DBLOCK_BITMAP, i);
            if (res == 0)
            {
                dblock_bitmap_last = i;
                return i;
            }
            i++;
            if (i == DATA_BLOCK_NUMBER)
                i = 0;
        }
    }
    else
    {
        i = (inode_bitmap_last + 1) % MAX_FILE_COUNT;
        while (i != inode_bitmap_last)
        {
            res = read_bitmap_block(INODE_BITMAP, i);
            if (res == 0)
            {
                inode_bitmap_last = i;
                return i;
            }
            i++;
            if (i == MAX_FILE_COUNT)
                i = 0;
        }
    }
    return -1;
}

// ==================== INODE INIT READ WRITE ====================

// Initializing an inode structure
static void inode_init(inode *p, int type) // 0 for dir , 1 for file
{
    p->size = 0;
    p->type = type;
    p->link_count = 1;
    bzero((char *)p->blocks, sizeof(uint16_t) * (DIRECT_BLOCK + 1));
}

// Write an inode to a specific index in the inode block
static void inode_write(int index, inode *inode_buff)
{
    char temp_block_scratch[NEW_BLOCK_SIZE];
    adapt_block_read(created_super_block->inode_start + (index / INODE_PER_BLOCK), temp_block_scratch);
    inode *inode_block_scratch = (inode *)temp_block_scratch;
    bcopy((unsigned char *)inode_buff, (unsigned char *)(inode_block_scratch + (index % INODE_PER_BLOCK)), sizeof(inode));
    adapt_block_write(created_super_block->inode_start + (index / INODE_PER_BLOCK), temp_block_scratch);
}

// Read inodes from a fs for various operations
static void inode_read(int index, inode *inode_buff)
{
    char temp_block_scratch[NEW_BLOCK_SIZE];
    adapt_block_read(created_super_block->inode_start + (index / INODE_PER_BLOCK), temp_block_scratch);
    inode *inode_block_scratch = (inode *)temp_block_scratch;
    bcopy((unsigned char *)(inode_block_scratch + (index % INODE_PER_BLOCK)), (unsigned char *)inode_buff, sizeof(inode));
}

static int inode_alloc(void)
{
    int search_res = -1;
    search_res = find_next_free(INODE_BITMAP);
    if (search_res >= 0)
    {
        write_bitmap_block(INODE_BITMAP, search_res, 1);
        created_super_block->inode_count++;
        sb_write();
        return search_res;
    }
    return -1;
}

static int inode_create(int type) // 0 for dir 1 for file , create and init !
{
    inode temp_inode;
    int alloc_index;
    alloc_index = inode_alloc();
    if (alloc_index < 0)
    {
        ERROR_MSG(("no enough inode space for new inode!\n"))
        return -1;
    }
    inode_init(&temp_inode, type);
    inode_write(alloc_index, &temp_inode);
    return alloc_index;
}

static void inode_free(int index) // free the inode, also free its data
{
    int temp = read_bitmap_block(INODE_BITMAP, index);
    inode inode_temp;
    if (temp)
    {
        inode_read(index, &inode_temp);
        int used_data_blocks; // total blocks used , not included indirect index block
        used_data_blocks = (inode_temp.size - 1 + NEW_BLOCK_SIZE) / NEW_BLOCK_SIZE;
        if (used_data_blocks > DIRECT_BLOCK) // use indirect block
        {
            // use scratch here
            dblock_read(inode_temp.blocks[DIRECT_BLOCK], block_copy);
            int i;
            for (i = 0; i <= DIRECT_BLOCK; i++)
                dblock_free(inode_temp.blocks[i]);
            uint16_t *block_list = (uint16_t *)block_copy;
            for (i = 0; i < used_data_blocks - DIRECT_BLOCK; i++)
                dblock_free(block_list[i]);
        }
        else
        {
            int i;
            for (i = 0; i < used_data_blocks; i++)
                dblock_free(inode_temp.blocks[i]);
        }
        write_bitmap_block(INODE_BITMAP, index, 0);
        created_super_block->inode_count--;
        sb_write();
    }
}

// ==================================================
static int dblock_alloc(void)
{
    int search_res = -1;
    search_res = find_next_free(DBLOCK_BITMAP);
    if (search_res >= 0)
    {
        write_bitmap_block(DBLOCK_BITMAP, search_res, 1);
        created_super_block->dblock_count++;
        sb_write();
        bzero_block_custom(created_super_block->dblock_start + search_res);
        return search_res;
    }
    ERROR_MSG(("alloc data block fail"))
    return -1;
}

//================================================================================
static int alloc_dblock_mount_to_inode(int inode_id)
{
    int alloc_res;
    inode temp;
    inode_read(inode_id, &temp);
    int next_block = (temp.size - 1 + NEW_BLOCK_SIZE) / NEW_BLOCK_SIZE; // start from 0 , mean the next in-inode blocks id
    if (next_block > MAX_BLOCKS_INDEX_IN_INODE)
    {
        ERROR_MSG(("beyond one inode can handle!\n"))
        return -1;
    }
    if (next_block >= DIRECT_BLOCK) // need indirect block
    {
        if (next_block == DIRECT_BLOCK) // need indirect block ,but the indirect index block has not been alloced
        {
            alloc_res = dblock_alloc();
            if (alloc_res < 0)
                return -1;
            temp.blocks[DIRECT_BLOCK] = alloc_res;
        }
        dblock_read(temp.blocks[DIRECT_BLOCK], block_copy);
        uint16_t *block_list = (uint16_t *)block_copy;

        alloc_res = dblock_alloc();
        if (alloc_res < 0)
        {
            if (next_block == DIRECT_BLOCK)
                dblock_free(temp.blocks[DIRECT_BLOCK]);
            return -1;
        }
        block_list[next_block - DIRECT_BLOCK] = alloc_res;
        dblock_write(temp.blocks[DIRECT_BLOCK], block_copy);
    }
    else
    {
        alloc_res = dblock_alloc();
        if (alloc_res < 0)
            return -1;
        temp.blocks[next_block] = alloc_res;
    }
    // ERROR_MSG(("inode %d need a block in-inode id %d, alloc_res %d\n",inode_id,next_block,alloc_res))
    inode_write(inode_id, &temp);
    return alloc_res;
}

// ================================================================================
// DIR ENTRY ADD
static int dir_entry_add(int dir_index, int son_index, char *filename)
{
    // Read father inode
    inode dir_inode;
    inode_read(dir_index, &dir_inode);

    // Next slot for entry
    int next_i;
    next_i = dir_inode.size / (sizeof(dir_entry));

    // Block index
    int next_i_inblock;
    next_i_inblock = next_i / DIR_ENTRY_PER_BLOCK;

    dir_entry new_entry;
    new_entry.inode_id = son_index;
    bzero(new_entry.file_name, MAX_FILE_NAME);
    strcpy_limited(filename, new_entry.file_name, MAX_FILE_NAME);

    if (next_i % DIR_ENTRY_PER_BLOCK == 0) // need new block
    {
        int alloc_res = alloc_dblock_mount_to_inode(dir_index);
        if (alloc_res < 0)
        {
            ERROR_MSG(("can't alloc dblock when insert dir_entry into dir\n"))
            return -1;
        }
        // update inode
        inode_read(dir_index, &dir_inode);

        dblock_read(alloc_res, block_copy);
        dir_entry *entry_list = (dir_entry *)block_copy;
        entry_list[0] = new_entry;
        dblock_write(alloc_res, block_copy);
    }
    else
    {
        if (next_i_inblock >= DIRECT_BLOCK)
        { // indirect block
            dblock_read(dir_inode.blocks[DIRECT_BLOCK], block_copy);
            uint16_t *block_list = (uint16_t *)block_copy;
            next_i_inblock = block_list[next_i_inblock - DIRECT_BLOCK]; // get real block no
        }
        else
        {
            // real block no is dir_inode.blocks[next_i_inblock]
            next_i_inblock = dir_inode.blocks[next_i_inblock];
        }

        dblock_read(next_i_inblock, block_copy);

        dir_entry *entry_list = (dir_entry *)block_copy;
        entry_list[next_i % DIR_ENTRY_PER_BLOCK] = new_entry;

        dblock_write(next_i_inblock, block_copy);
    }

    dir_inode.size += sizeof(dir_entry);
    inode_write(dir_index, &dir_inode); // update dir inode
    return 0;
}

// ---------- COMMANDS ----------
// ---------- COMMANDS ----------
// ---------- COMMANDS ----------
// ---------- COMMANDS ----------

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
    return -1;
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

void fs_ls()
{
    inode dir_inode;
    dir_entry dir_entries[NEW_BLOCK_SIZE / sizeof(dir_entry)];

    inode_read(pwd, &dir_inode);

    int i, j;

    printf(".\n");
    printf("..\n");

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
}
