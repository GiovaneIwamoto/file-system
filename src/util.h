#ifndef UTIL_INCLUDED
#define UTIL_INCLUDED
#include "common.h"

int atoi(const char *s);
int strlen(const char *s );

void clear_screen(int minx, int miny, int maxx, int maxy);
void scroll(int minx, int miny, int maxx, int maxy);
int peek_screen(int x, int y);

void		ms_delay(uint32_t msecs);
uint64_t	get_timer( void);

void itoa(int n, char *s);
void itohex(unsigned int n, char *s);

void print_char(int line, int col, char c);
void print_int(int line, int col, int num);
void print_hex(int line, int col, unsigned int num);
void print_str(int line, int col, char *str);

void reverse(char *s);

int same_string(char *s1, char *s2);

void bcopy(unsigned char *source, unsigned char *destin, int size);
void bzero(char *a, int size);

unsigned char inb(int port);
void outb(int port, unsigned char data);
void iodelay(void);

void dprint(char *str);
#endif
