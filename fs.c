#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

char *buffer;

void fs_init(void)
{
    int magic_number = 0;

    block_init(); // Block Init

    buffer_alloc();
    block_read(SUPER_BLOCK, buffer); // Read Super Block

    magic_number = byte_to_int(buffer, BLOCK_SIZE - 4);

    if (magic_number != MAGIC_NUMBER)
    {
        printf("Unformatted disk\n");
        printf("Formatting...\n");
        fs_mkfs();
    }
    else
    {
        printf("Disk is already formatted\n");
    }
}

int fs_mkfs(void)
{
    int volume_size = FS_SIZE; // 2048

    // Data Blocks = Volume - SuperBlock - BlockAllocationMap - Inodes
    int data_blocks = (volume_size - INODES - 2);

    block_read(SUPER_BLOCK, buffer);

    // TODO:

    block_write(SUPER_BLOCK, buffer);

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
    return -1;
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

int byte_to_int(char *buffer, int position)
{
    int i, integer = 0, bits_offset = 24;
    for (i = 0; i < 4; i++)
    {
        integer |= buffer[position + i] << bits_offset;
        bits_offset -= 8;
    }
    return integer;
}

void buffer_alloc(void)
{
    if (!buffer)
    {
        buffer = (char *)malloc(BLOCK_SIZE * sizeof(char));
        if (!buffer)
        {
            printf("Error allocating buffer\n");
            exit(EXIT_FAILURE);
        }
    }
}