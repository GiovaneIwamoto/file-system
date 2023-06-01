#include "common.h"
#include "util.h"

static short *screen = SCREEN_ADDR;

void clear_screen(int minx, int miny, int maxx, int maxy)
{
    int i, j, k;

    for (j = miny; j < maxy; j++) {
	for (i = minx; i < maxx; i++) {
	    k = j * 80 + i;
	    screen[k] = 0x0700;
	}
    }
}

void scroll(int minx, int miny, int maxx, int maxy)
{
    int i, j, k;

    for (j = miny; j < maxy; j++) {
	for (i = minx; i < maxx; i++) {
	    k = j * 80 + i;
	    if (j < maxy - 1) {
		screen[k] = screen[k + 80];
	    } else {
		screen[k] = 0x0700;
	    }
	}
    }
}

int peek_screen(int x, int y)
{
    return screen[y * 80 + x] & 0xff;
}

/*
void delay(int n)
{
    int i, j;

    for (i = 0; i < n; i++) {
	j = i * 11;
    }
}
*/

void 
itoa( int n, char *s) {
    int i, positive = TRUE;

    i = 0;
    if ( n < 0) {
        positive = FALSE;
	n = -n;
    }
    do {
	s[i++] = n %10 + '0';
    } while((n /= 10) >0) ;
    if ( !positive)
	s[i++] = '-';
    s[i++] = 0;
    reverse(s);
}

int 
atoi( const char *s) {
    int n = 0, sign = 1;
    
    if ( *s == '-') {
	sign = -1;
	s++;
    }
    while ( *s) {
	n = n * 10 + (*s) - '0';
	s++;
    }
    return sign * n;
}

void itohex(unsigned int n, char *s)
{
    int i, d;

    i = 0;
    do {
	d = n % 16;
	if (d < 10) {
	    s[i++] = d + '0';
	} else {
	    s[i++] = d - 10 + 'a';
	}
    } while ((n /= 16) > 0);
    s[i++] = 0;
    reverse(s);
}

void print_char(int line, int col, char c)
{
    if ((line < 0) || (line > 24)) {
	return;
    }
    if ((col < 0) || (col > 79)) {
	return;
    }
    screen[line * 80 + col] = 0x07 << 8 | c;
}

void print_int(int line, int col, int num)
{
    int i, n, neg_flag;
    char buf[12];

    neg_flag = num < 0;
    if (neg_flag) {
	num = ~num + 1;
    }
    itoa(num, buf);

    n = strlen(buf);
    if (neg_flag) {
	print_char(line, col++, '-');
    }
    for (i = 0; i < n; i++) {
	print_char(line, col++, buf[i]);
    }
}

void print_hex(int line, int col, unsigned int num)
{
    int i, n;
    char buf[12];

    itohex(num, buf);

    n = strlen(buf);
    for (i = 0; i < n; i++) {
	print_char(line, col + i, buf[i]);
    }
}

void print_str(int line, int col, char *str)
{
    int i, n;

    n = strlen(str);
    for (i = 0; i < n; i++) {
	print_char(line, col + i, str[i]);
    }
}

void reverse(char *s)
{
    int c, i, j;

    for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
	c = s[i];
	s[i] = s[j];
	s[j] = c;
    }
}


int strlen(const char *s)
{
    int n;

    for (n = 0; *s != '\0'; s++) {
	n++;
    }
    return n;
}


int same_string(char *s1, char *s2)
{
    while ((*s1 != 0) && (*s2 != 0)) {
	if (*s1 != *s2) {
	    return FALSE;
	}
	s1++;
	s2++;
    }
    return (*s1 == *s2);
}

void bcopy(unsigned char *source, unsigned char *destin, int size)
{
    int i;

    if (size == 0) {
	return;
    }
    if (source < destin) {
	for (i = size - 1; i >= 0; i--) {
	    destin[i] = source[i];
	}
    } else {
	for (i = 0; i < size; i++) {
	    destin[i] = source[i];
	}
    }
}

void bzero(char *area, int size)
{
    while (size) {
	area[--size] = 0;
    }
}

/* read byte from I/O address space */
uchar_t inb(int port)
{
    int ret;

//    iodelay();
    asm volatile ("xorl %eax,%eax");
    asm volatile ("inb %%dx,%%al":"=a" (ret):"d"(port));

    return ret;
}

/* write byte to I/O address space */
void outb(int port, uchar_t data)
{
//    iodelay();
    asm volatile ("outb %%al,%%dx"::"a" (data), "d"(port));
}

/* This is the delay needed between each access to the I/O address
 * space.  The delay must be tuned according to processor speed (the
 * number we use should be safe within the 486 family). */
void iodelay(void)
{
    int i;

    for (i = 0; i < 10000; i++);
}

#ifndef FAKE 
//	define a local prototype to int cpuspeed(), so we don't get any compiler warnings
int	cpuspeed(void);

static uint64_t ms_to_cycles(uint32_t msecs) {
	static uint32_t		cpu_spd = 0;
	uint64_t	result;
	
	if (!cpu_spd)
		cpu_spd	= cpuspeed();
	
	result		= cpu_spd * 1000;	// cycles pr ms : (MHz * 10 ^ 6) * 10 ^ -3 = MHz * 10^3
	result		*= msecs;
	return result;
}

/*	Wait for atleast <msecs> number of milliseconds. Does not handle
	the counter overflowing.
*/
void		ms_delay(uint32_t msecs) {
	uint64_t	cur_time, end_time;
	
	cur_time	= get_timer();
	end_time	= cur_time + ms_to_cycles(msecs);
	while (cur_time < end_time) {
		cur_time	= get_timer();
	}
}
#endif

/* Read the pentium time stamp counter */
uint64_t	get_timer(void) {
	uint64_t	x = 0LL;

	/* Load the time stamp counter into edx:eax (x) */
	asm volatile ("rdtsc":"=A" (x));
	
	return x;
}

void dprint (char *str)
{
    static int base_line = 0;
    print_str(base_line, 0, str);
    if (++base_line > 20)
	base_line = 0;
}
