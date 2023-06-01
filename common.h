/*	common.h

	Common definitions and types.
	Best viewed with tabs set to 4 spaces.
*/

#ifndef COMMON_H
#define COMMON_H

#ifndef NULL
#define NULL ((void*) 0)
#endif

//	Size of a sector on the floppy 
#define SECTOR_SIZE 512

//	Physical address of the text mode display 
#define SCREEN_ADDR ((short *) 0xb8000)

//	Unique integers for each system call 
enum {
	SYSCALL_YIELD,       /* 0 */
	SYSCALL_EXIT,
	SYSCALL_GETPID,
	SYSCALL_GETPRIORITY,
	SYSCALL_SETPRIORITY,
	SYSCALL_CPUSPEED,    /* 5 */
	SYSCALL_MBOX_OPEN,
	SYSCALL_MBOX_CLOSE,
	SYSCALL_MBOX_STAT,
	SYSCALL_MBOX_RECV,
	SYSCALL_MBOX_SEND,   /* 10 */
	SYSCALL_GETCHAR,
	SYSCALL_FSCK,
	SYSCALL_MKFS,
	SYSCALL_OPEN,
	SYSCALL_CLOSE,   /* 15 */
	SYSCALL_READ, 
	SYSCALL_WRITE,
	SYSCALL_LSEEK,
	SYSCALL_MKDIR,
	SYSCALL_RMDIR,   /* 20 */
	SYSCALL_CD,
	SYSCALL_LINK,
	SYSCALL_UNLINK,
	SYSCALL_STAT,
	SYSCALL_READDIR,  /* 25 */
	SYSCALL_LOADPROC,
	SYSCALL_WRITE_SERIAL,
	SYSCALL_COUNT
};


/*	If the expression p fails, print the source file and 
	line number along with the text s. Then hang the os. 

	Processes should use this macro.
*/
#define ASSERT2(p, s) \
	do { \
	if (!(p)) { \
		print_str(0, 0, "Assertion failure: "); \
		print_str(0, 19, s); \
		print_str(1, 0, "file: "); \
		print_str(1, 6, __FILE__); \
		print_str(2, 0, "line: "); \
		print_int(2, 6, __LINE__); \
		asm volatile ("cli"); \
		while (1); \
	} \
	} while(0)

/*	The #p tells the compiler to copy the expression p 
	into a text string and use this as an argument. 
	=> If the expression fails: print the expression as a message. 
*/
#define ASSERT(p) ASSERT2(p, #p)

//	Gives us the source file and line number where we decide to hang the os. 
#define HALT(s) ASSERT2(FALSE, s)

//	Typedefs of commonly used types
typedef enum {
	FALSE, TRUE
} bool_t;

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
#ifndef FAKE
typedef long long int int64_t;
#endif

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

typedef unsigned char uchar_t;
typedef unsigned int uint_t;
typedef unsigned long ulong_t;

#define FREE_INODE 0
#define DIRECTORY 1
#define FILE_TYPE 2

#define FS_O_RDONLY 1
#define FS_O_WRONLY 2
#define FS_O_RDWR 3

typedef struct {
    // Fill in your stat here, this is just an example
    int inodeNo;        /* the file i-node number */
    short type;         /* the file i-node type, DIRECTORY, FILE_TYPE (there's another value FREE_INODE which never appears here */
    char links;         /* number of links to the i-node */
    int size;           /* file size in bytes */
    int numBlocks;      /* number of blocks used by the file */
} fileStat;

/*	Note that this struct only allocates space for the size element.

	To use a message with a body of 50 bytes we must first allocate space for 
	the message:
		char space[sizeof(int) + 50];
	Then we declares a msg_t * variable that points to the allocated space:
		msg_t *m = (msg_t *) space;
	We can now access the size variable:
		m->size (at memory location &(space[0]) )
	And the 15th character in the message:
		m->body[14] (at memory location &(space[0]) + sizeof(int) + 14 * sizeof(char))
*/
typedef struct {
	int size;		//	Size of message contents in bytes
	char body[0];	//	Pointer to start of message contents
} __attribute__ ((packed)) msg_t;

//	Return size of header
#define MSG_T_HEADER_SIZE (sizeof(int))
//	Return size of message including header
#define MSG_SIZE(m) (MSG_T_HEADER_SIZE + m->size)

struct directory_t {
	int location;	//	Sector number
	int size;		//	Size in number of sectors
};

#endif
