CC = gcc
LD = ld

CFLAGS = -fno-builtin-strlen -fno-builtin-bcopy -fno-builtin-bzero

CCOPTS = -Wall -O1 -c

FAKESHELL_OBJS = shellFake.o shellutilFake.o utilFake.o fsFake.o blockFake.o

# Makefile targets
all: lnxsh

lnxsh: $(FAKESHELL_OBJS)
	$(CC) -o lnxsh $(FAKESHELL_OBJS)

shellFake.o : shell.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o shellFake.o shell.c

shellutilFake.o : shellutilFake.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o shellutilFake.o shellutilFake.c

blockFake.o : blockFake.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o blockFake.o blockFake.c

utilFake.o : util.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o utilFake.o util.c

fsFake.o : fs.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o fsFake.o fs.c

# Figure out dependencies, and store them in the hidden file .depend
depend: .depend
.depend:
	$(CC) $(CCOPTS) -MM *.c > $@

# Clean up!
clean:
	rm -f *.o
	rm -f lnxsh
	rm -f .depend

# No, really, clean up!
distclean: clean
	rm -f *~
	rm -f \#*
	rm -f *.bak

# How to compile a C file
%.o:%.c
	$(CC) $(CCOPTS) $<

# How to assemble
%.o:%.s
	$(CC) $(CCOPTS) $<

# How to produce assembler input from a C file
%.s:%.c
	$(CC) $(CCOPTS) -S $<

# Include dependencies (the leading dash prevents warnings if the file doesn't exist)
-include .depend
