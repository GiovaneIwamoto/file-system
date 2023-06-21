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

static void write_bitmap_block(int inode_or_dt, int index, int val) // 0 for inode bitmap,1 for data bitmap
{
    char *bitmap_block_scratch;
    if (inode_or_dt)
        bitmap_block_scratch = dblock_bitmap_block_copy;
    else
        bitmap_block_scratch = inode_bitmap_block_copy;

    int byte_index = index / 8; // Byte index calc
    uint8_t the_byte = bitmap_block_scratch[byte_index];

    int mask_off = index % 8; // Bit offset within byte
    uint8_t mask = 1 << mask_off;

    the_byte = the_byte & (~mask); // Set or clear bit
    if (val)
        the_byte = the_byte | mask;

    bitmap_block_scratch[byte_index] = the_byte; // Update

    // Write the modified bitmap block back to the corresponding location
    if (inode_or_dt)
        adapt_block_write(created_super_block->dblock_bitmap_place, bitmap_block_scratch);
    else
        adapt_block_write(created_super_block->inode_bitmap_place, bitmap_block_scratch);
}

static int read_bitmap_block(int inode_or_dt, int index) // 0 for inode bitmap,1 for data bitmap
{
    char *bitmap_block_scratch;

    if (inode_or_dt)
        bitmap_block_scratch = dblock_bitmap_block_copy; // Assign the data bitmap block copy

    else
        bitmap_block_scratch = inode_bitmap_block_copy; // Assign the inode bitmap block copy

    int byte_index = index / 8; // Byte index
    uint8_t the_byte = bitmap_block_scratch[byte_index];

    int mask_off = index % 8;
    uint8_t mask = 1 << mask_off;

    return (mask & the_byte) ? 1 : 0; // Ckeck targ bit is set in byte
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
        created_super_block->dblock_count--; // Decrease count
        sb_write();                          // Write changes to disk
    }
    write_bitmap_block(DBLOCK_BITMAP, index, 0);
}

// ==================== FIND AVAILABLE ====================

static int find_available(int inode_or_dt)
{
    int i;
    int res;
    if (inode_or_dt)
    { // Find free data block
        i = (dblock_bitmap_last + 1) % DATA_BLOCK_NUMBER;
        while (i != dblock_bitmap_last)
        {
            res = read_bitmap_block(DBLOCK_BITMAP, i);
            if (res == 0)
            {
                dblock_bitmap_last = i; // Update last allc
                return i;
            }
            i++;
            if (i == DATA_BLOCK_NUMBER)
                i = 0;
        }
    }
    else
    { // Next free inode
        i = (inode_bitmap_last + 1) % MAX_FILE_COUNT;
        while (i != inode_bitmap_last)
        {
            res = read_bitmap_block(INODE_BITMAP, i);
            if (res == 0)
            {
                inode_bitmap_last = i; // Ipdate last allc
                return i;
            }
            i++;
            if (i == MAX_FILE_COUNT)
                i = 0;
        }
    }
    return -1;
}

// ==================== INODE INIT READ ALLOC WRITE FREE ====================

// Initializing an inode structure
static void inode_init(inode *prop, int type)
{
    // Initializes an inode structure by setting its properties
    prop->size = 0;
    prop->type = type;
    prop->link_count = 1;
    // Clears the block pointers in the inode
    bzero((char *)prop->blocks, sizeof(uint16_t) * (DIRECT_BLOCK + 1));
}

// Write an inode to a specific index in the inode block
static void inode_write(int index, inode *inode_buff)
{
    char temp_block_scratch[NEW_BLOCK_SIZE];
    adapt_block_read(created_super_block->inode_start + (index / INODE_PER_BLOCK), temp_block_scratch);
    inode *inode_block_scratch = (inode *)temp_block_scratch;

    // Copy of contents of the inode buffer to the target index in the temporary inode block
    bcopy((unsigned char *)inode_buff, (unsigned char *)(inode_block_scratch + (index % INODE_PER_BLOCK)), sizeof(inode));
    adapt_block_write(created_super_block->inode_start + (index / INODE_PER_BLOCK), temp_block_scratch);
}

// Read inodes from a fs for various operations
static void inode_read(int index, inode *inode_buff)
{
    char temp_block_scratch[NEW_BLOCK_SIZE];
    adapt_block_read(created_super_block->inode_start + (index / INODE_PER_BLOCK), temp_block_scratch);
    inode *inode_block_scratch = (inode *)temp_block_scratch;

    // Copy contents of the tgt index in the temporary inode block to the inode buffer
    bcopy((unsigned char *)(inode_block_scratch + (index % INODE_PER_BLOCK)), (unsigned char *)inode_buff, sizeof(inode));
}

static int inode_alloc(void)
{
    int searched = -1;
    searched = find_available(INODE_BITMAP); // Find the next free inode by searching the inode bitmap

    if (searched >= 0) // Free inode found
    {
        write_bitmap_block(INODE_BITMAP, searched, 1);
        created_super_block->inode_count++; // Allocation counter added
        sb_write();                         // Wb

        return searched;
    }
    return -1;
}

static int inode_create(int type)
{
    inode inode_copy; // Inode structure hold new inode info
    int i_allocated_index;

    i_allocated_index = inode_alloc();

    if (i_allocated_index < 0) // Fail
    {
        return -1;
    }
    inode_init(&inode_copy, type);
    inode_write(i_allocated_index, &inode_copy);

    return i_allocated_index;
}

static void inode_free(int index)
{
    int temp_stat = read_bitmap_block(INODE_BITMAP, index); // Check if the inode is marked as used in the inode bitmap
    inode inode_temp;                                       // Inode struct

    if (temp_stat) // If the inode is marked as used
    {
        inode_read(index, &inode_temp); // Read the inode from the specified index into the temporary inode structure
        int used_data_blocks;
        used_data_blocks = (inode_temp.size - 1 + NEW_BLOCK_SIZE) / NEW_BLOCK_SIZE;

        if (used_data_blocks > DIRECT_BLOCK) // If the number of used data blocks exceeds the number of direct blocks
        {
            dblock_read(inode_temp.blocks[DIRECT_BLOCK], block_copy);
            int i;

            for (i = 0; i <= DIRECT_BLOCK; i++)
                dblock_free(inode_temp.blocks[i]); // Free the direct blocks

            uint16_t *block_list = (uint16_t *)block_copy;

            for (i = 0; i < used_data_blocks - DIRECT_BLOCK; i++)
                dblock_free(block_list[i]);
        }
        else
        {
            int i;
            for (i = 0; i < used_data_blocks; i++)
                dblock_free(inode_temp.blocks[i]); // Free all the used data blocks
        }
        write_bitmap_block(INODE_BITMAP, index, 0); // Mark the inode as free in the inode bitmap
        created_super_block->inode_count--;
        sb_write(); // Update
    }
}

// ==================== DATA BLOCK ALLOCATION ====================

static int dblock_alloc(void)
{
    int search_res = -1;
    search_res = find_available(DBLOCK_BITMAP); // Find aval

    if (search_res >= 0) // If a free data block is found
    {
        write_bitmap_block(DBLOCK_BITMAP, search_res, 1); // Mark the data block as used in the data block bitmap
        created_super_block->dblock_count++;
        sb_write();

        bzero_block_custom(created_super_block->dblock_start + search_res);
        return search_res;
    }
    ERROR_MSG(("Impossible to alloc."))
    return -1;
}

// ==================== ALLOC DATA BLOCK MOUNT TO INODE ====================
// TODO:
static int alloc_dblock_mount_to_inode(int inode_id)
{
    int alloc_res;
    inode temp;
    inode_read(inode_id, &temp);
    int next_block = (temp.size - 1 + NEW_BLOCK_SIZE) / NEW_BLOCK_SIZE;
    if (next_block > MAX_BLOCKS_INDEX_IN_INODE)
    {
        return -1;
    }
    if (next_block >= DIRECT_BLOCK)
    {
        if (next_block == DIRECT_BLOCK)
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

    inode_write(inode_id, &temp);
    return alloc_res;
}

// ==================== DIRECTORY ENTRY ADD ====================

static int dir_entry_add(int dir_index, int son_index, char *filename)
{
    inode dir_inode;
    inode_read(dir_index, &dir_inode);

    int next_i;
    next_i = dir_inode.size / (sizeof(dir_entry));

    int next_i_inblock;
    next_i_inblock = next_i / DIR_ENTRY_PER_BLOCK;

    dir_entry new_entry;
    new_entry.inode_id = son_index;
    bzero(new_entry.file_name, MAX_FILE_NAME);
    strcpy_limited(filename, new_entry.file_name, MAX_FILE_NAME);

    if (next_i % DIR_ENTRY_PER_BLOCK == 0)
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
        {
            dblock_read(dir_inode.blocks[DIRECT_BLOCK], block_copy);
            uint16_t *block_list = (uint16_t *)block_copy;
            next_i_inblock = block_list[next_i_inblock - DIRECT_BLOCK]; // get real block no
        }
        else
        {
            next_i_inblock = dir_inode.blocks[next_i_inblock];
        }

        dblock_read(next_i_inblock, block_copy);

        dir_entry *entry_list = (dir_entry *)block_copy;
        entry_list[next_i % DIR_ENTRY_PER_BLOCK] = new_entry;

        dblock_write(next_i_inblock, block_copy);
    }

    dir_inode.size += sizeof(dir_entry);
    inode_write(dir_index, &dir_inode);
    return 0;
}

static int dir_entry_find(int dir_index, char *filename)
{
    inode dir_inode;
    inode_read(dir_index, &dir_inode);
    int total_entry_num = dir_inode.size / (sizeof(dir_entry));
    int total_block_num = (total_entry_num - 1 + DIR_ENTRY_PER_BLOCK) / DIR_ENTRY_PER_BLOCK;
    if (total_entry_num == 0)
        return -1;

    if (total_block_num > DIRECT_BLOCK)
    {
        int i, j;

        dblock_read(dir_inode.blocks[DIRECT_BLOCK], block_copy);
        uint16_t *block_list = (uint16_t *)block_copy;

        for (i = 0; i < total_block_num - DIRECT_BLOCK - 1; i++)
        {
            dblock_read(block_list[i], block_copy_copy);
            dir_entry *entry_list = (dir_entry *)block_copy_copy;
            for (j = 0; j < DIR_ENTRY_PER_BLOCK; j++)
                if (same_string(entry_list[j].file_name, filename))
                    return entry_list[j].inode_id;
        }
        dblock_read(block_list[total_block_num - DIRECT_BLOCK - 1], block_copy_copy);
        int final_end = (total_entry_num - 1) % DIR_ENTRY_PER_BLOCK;
        dir_entry *entry_list = (dir_entry *)block_copy_copy;
        for (j = 0; j <= final_end; j++)
        {
            if (same_string(entry_list[j].file_name, filename))
                return entry_list[j].inode_id;
        }

        total_block_num = DIRECT_BLOCK;
        total_entry_num = DIRECT_BLOCK * DIR_ENTRY_PER_BLOCK;
    }
    int i, j, entry_block;
    for (i = 0; i < total_block_num - 1; i++)
    {
        entry_block = dir_inode.blocks[i];
        dblock_read(entry_block, block_copy);
        dir_entry *entry_list = (dir_entry *)block_copy;
        for (j = 0; j < DIR_ENTRY_PER_BLOCK; j++)
            if (same_string(entry_list[j].file_name, filename))
                return entry_list[j].inode_id;
    }
    int final_end = (total_entry_num - 1) % DIR_ENTRY_PER_BLOCK;
    entry_block = dir_inode.blocks[total_block_num - 1];
    dblock_read(entry_block, block_copy);
    dir_entry *entry_list = (dir_entry *)block_copy;
    for (j = 0; j <= final_end; j++)
    {
        if (same_string(entry_list[j].file_name, filename))
            return entry_list[j].inode_id;
    }

    return -1;
}

//==================== FD OPERATIONS ====================

static int fd_open(int inode_id, int mode)
{
    int i;
    for (i = 0; i < MAX_FILE_OPEN; i++)
        if (file_desc_table[i].is_using == FALSE)
        {
            file_desc_table[i].is_using = TRUE;
            file_desc_table[i].cursor = 0;
            file_desc_table[i].inode_id = inode_id;
            file_desc_table[i].mode = mode;
            return i;
        }
    ERROR_MSG(("Not enough file descriptor!\n"))
    return -1;
}
static void fd_close(int fd)
{
    file_desc_table[fd].is_using = FALSE;
}
static int fd_find_same_num(int inode_id)
{
    int i;
    int count = 0;
    for (i = 0; i < MAX_FILE_OPEN; i++)
        if (file_desc_table[i].is_using && file_desc_table[i].inode_id == inode_id)
            count++;
    return count;
}

// ==================== PATH RESOLVE ====================

static int rel_path_dir_resolve(char *file_path, int temp_pwd) // this just resolve relative path and find inode
{
    int path_len = strlen(file_path);
    if (path_len <= 0)
    {
        return temp_pwd;
    }
    int i;
    for (i = 0; i < path_len; i++)
        if (file_path[i] == '/')
        {
            file_path[i] = '\0';
            break;
        }
    int res = dir_entry_find(temp_pwd, file_path);
    if (res < 0)
    {
        return -1;
    }
    if (i == path_len)
        return res;
    inode temp;
    inode_read(res, &temp);
    if (temp.type != POS_DIRECTORY)
    {
        ERROR_MSG(("%s is a data file not a path!\n", file_path))
        return -1;
    }
    return rel_path_dir_resolve(file_path + i + 1, res);
}

static int path_resolve(char *file_path, int temp_pwd, int mode)
{
    if (file_path == NULL)
    {
        ERROR_MSG(("No path input!\n"))
        return -1;
    }
    int path_len = strlen(file_path);
    if (path_len > MAX_PATH_NAME)
    {
        ERROR_MSG(("too long path!\n"))
        return -1;
    }
    if (path_len == 0)
    {
        ERROR_MSG(("no path input!\n"))
        return -1;
    }

    char path_buffer[MAX_PATH_NAME + 1];
    bcopy((unsigned char *)file_path, (unsigned char *)path_buffer, path_len);
    path_buffer[path_len] = '\0';
    file_path = path_buffer;

    if (file_path[0] == '/')
    {
        temp_pwd = PWD_ID_ROOT_DIR;
        file_path++;
        path_len--;
    }

    if (mode == 2)
    {
        int i;
        for (i = path_len - 1; i >= 0; i--)
            if (file_path[i] == '/')
            {
                file_path[i + 1] = '\0';
                break;
            }
        if (i == -1)
            return temp_pwd;
        else if (i == 0)
            return PWD_ID_ROOT_DIR;
        else
            return path_resolve(file_path, temp_pwd, POS_DIRECTORY);
    }

    if (mode != POS_DIRECTORY)
    {
        if (path_len == 0 || file_path[path_len - 1] == '/')
        {
            ERROR_MSG(("try to find a file but input a path!\n"))
            return -1;
        }
    }
    else if (file_path[path_len - 1] == '/')
        file_path[path_len - 1] = '\0';

    return rel_path_dir_resolve(file_path, temp_pwd);
}