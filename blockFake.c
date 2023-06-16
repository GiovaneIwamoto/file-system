#include <stdio.h>
#include <assert.h>
#include "common.h"
#include "block.h"

static FILE *fd;

#include <errno.h>

void block_init(void)
{
	int ret;

	fd = fopen("./disk", "r+");
	if (fd == NULL)
		fd = fopen("./disk", "w+");
	assert(fd);

	ret = fseek(fd, 0, SEEK_SET);
	assert(ret == 0);
}

void block_read(int block, char *mem)
{
	int ret;

	ret = fseek(fd, block * BLOCK_SIZE, SEEK_SET);
	assert(ret == 0);

	ret = fread(mem, 1, BLOCK_SIZE, fd);
	if (ret == 0)
	{ /* End of file */
		ret = BLOCK_SIZE;
		bzero_block(mem);
	}
	assert(ret == BLOCK_SIZE);
}

void block_write(int block, char *mem)
{
	int ret;

	ret = fseek(fd, block * BLOCK_SIZE, SEEK_SET);
	assert(ret == 0);

	ret = fwrite(mem, 1, BLOCK_SIZE, fd);
	assert(ret == BLOCK_SIZE);
}

void bzero_block(char *block)
{
	int i;

	for (i = 0; i < BLOCK_SIZE; i++)
		block[i] = 0;
}
