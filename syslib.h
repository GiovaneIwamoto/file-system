/*	syslib.h
	Best viewed with tabs set to 4 spaces.
*/
#ifndef SYSLIB_H
	#define SYSLIB_H

//	Includes
	#include "common.h"

//	Constants
enum {
	IGNORE		= 0
};	

int fs_mkfs( void);
int fs_open( char *filename, int flags);
int fs_close( int fd);
int fs_read( int fd, char *buf, int count);
int fs_write( int fd, char *buf, int count);
int fs_lseek( int fd, int offset);
int fs_mkdir( char *fileName);
int fs_rmdir( char *fileName);
int fs_cd( char *pathName);
int fs_link( char *pathName, char *fileName);
int fs_unlink( char *fileName);
int fs_stat( char *fileName, fileStat *buf);

#endif
