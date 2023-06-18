#include "fs.c"

// ---------- AUX FUNCTIONS ----------

static void strcpy_safe(char *src, char *dest, int dest_max_len) // dest max len without final '\0' buffer
{
    if (strlen(src) > dest_max_len)
        bcopy((unsigned char *)src, (unsigned char *)dest, dest_max_len);
    else
        bcopy((unsigned char *)src, (unsigned char *)dest, strlen(src));
    dest[dest_max_len] = '\0';
}

// Read multiple blocks of data from fs.
static void adapt_block_read(int block, char *mem)
{
    int i;
    for (i = 0; i < NEW_BLOCK_SIZE / BLOCK_SIZE; i++)
    {
        block_read(block * 8 + i, mem + i * BLOCK_SIZE); // Uses block_read
    }
}

static void adapt_block_write(int block, char *mem)
{
    int i;
    for (i = 0; i < NEW_BLOCK_SIZE / BLOCK_SIZE; i++)
    {
        block_write(block * 8 + i, mem + i * BLOCK_SIZE);
    }
}

static void inode_init(inode *p, int type) // 0 for dir , 1 for file
{
    p->size = 0;
    p->type = type;
    p->link_count = 1;
    bzero((char *)p->blocks, sizeof(uint16_t) * (DIRECT_BLOCK + 1));
}

static void inode_write(int index, inode *inode_buff)
{
    char temp_block_scratch[NEW_BLOCK_SIZE];
    adapt_block_read(created_super_block->inode_start + (index / INODE_PER_BLOCK), temp_block_scratch);
    inode *inode_block_scratch = (inode *)temp_block_scratch;
    bcopy((unsigned char *)inode_buff, (unsigned char *)(inode_block_scratch + (index % INODE_PER_BLOCK)), sizeof(inode));
    adapt_block_write(created_super_block->inode_start + (index / INODE_PER_BLOCK), temp_block_scratch);
}

// caller prepare space for inode and check valid
static void inode_read(int index, inode *inode_buff)
{
    char temp_block_scratch[NEW_BLOCK_SIZE];
    adapt_block_read(created_super_block->inode_start + (index / INODE_PER_BLOCK), temp_block_scratch);
    inode *inode_block_scratch = (inode *)temp_block_scratch;
    bcopy((unsigned char *)(inode_block_scratch + (index % INODE_PER_BLOCK)), (unsigned char *)inode_buff, sizeof(inode));
}

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

static int dblock_alloc(void)
{
    int search_res = -1;
    search_res = find_next_free(DBLOCK_BITMAP);
    if (search_res >= 0)
    {
        write_bitmap_block(DBLOCK_BITMAP, search_res, 1);
        created_super_block->dblock_count++;
        sb_write();
        my_bzero_block(created_super_block->dblock_start + search_res);
        return search_res;
    }
    ERROR_MSG(("alloc data block fail"))
    return -1;
}

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
    strcpy_safe(filename, new_entry.file_name, MAX_FILE_NAME);

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