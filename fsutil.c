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

// ==================== FIND NEXT FREE ====================

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

// ==================== INODE INIT READ ALLOC WRITE FREE ====================

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

// ==================== DATA BLOCK ALLOCATION ====================

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

// ==================== ALLOC DATA BLOCK MOUNT TO INODE ====================

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

// ==================== DIRECTORY ENTRY ADD ====================

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
        // last block
        dblock_read(block_list[total_block_num - DIRECT_BLOCK - 1], block_copy_copy);
        int final_end = (total_entry_num - 1) % DIR_ENTRY_PER_BLOCK;
        dir_entry *entry_list = (dir_entry *)block_copy_copy;
        for (j = 0; j <= final_end; j++)
        {
            if (same_string(entry_list[j].file_name, filename))
                return entry_list[j].inode_id;
        }

        // trick to find in direct blocks
        total_block_num = DIRECT_BLOCK;
        total_entry_num = DIRECT_BLOCK * DIR_ENTRY_PER_BLOCK;
    }
    // direct blocks
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
    // last block
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
        // ERROR_MSG(("%s doesn't exist!\n",file_path))
        return -1;
    }
    if (i == path_len) // we reach the last item
        return res;
    inode temp;
    inode_read(res, &temp);
    if (temp.type != MY_DIRECTORY)
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

    // copy the path
    char path_buffer[MAX_PATH_NAME + 1];
    bcopy((unsigned char *)file_path, (unsigned char *)path_buffer, path_len);
    path_buffer[path_len] = '\0';
    file_path = path_buffer;

    if (file_path[0] == '/') // absolute path
    {
        // trick to change str and temp_pwd
        temp_pwd = PWD_ID_ROOT_DIR;
        file_path++;
        path_len--;
    }

    if (mode == 2) // to find the last dir
    {
        int i;
        for (i = path_len - 1; i >= 0; i--) // cut the last term
            if (file_path[i] == '/')
            {
                file_path[i + 1] = '\0';
                break;
            }
        // find dir
        if (i == -1)
            return temp_pwd;
        else if (i == 0)
            return PWD_ID_ROOT_DIR;
        else
            return path_resolve(file_path, temp_pwd, MY_DIRECTORY);
    }

    // real file or last dir mode need to check last char
    if (mode != MY_DIRECTORY)
    {
        if (path_len == 0 || file_path[path_len - 1] == '/') // root or end up with /
        {
            ERROR_MSG(("try to find a file but input a path!\n"))
            return -1;
        }
    }
    else if (file_path[path_len - 1] == '/') // dir allow input: cd abc/ or cd abc
        file_path[path_len - 1] = '\0';

    // now we have a prepared relative path
    return rel_path_dir_resolve(file_path, temp_pwd);
}