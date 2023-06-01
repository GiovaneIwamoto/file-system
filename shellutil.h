#ifndef SHELLUTIL_INCLUDED
#define SHELLUTIL_INCLUDED

#define MINX 30
#define SIZEX 50
#define MINY 0
#define SIZEY 7
#define MAX_LINE (SIZEX-2)
#define RETURN 13
#define RETURN_R 13
#define RETURN_N 10

void shell_init( void);
void writeChar( int c);
void readChar( int *c);
void writeStr( char *s);
void writeInt( int i );
void clearShellScreen( void);
void fire( void);

#endif
