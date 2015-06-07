CC=gcc
LIBS=-lncurses

cscribe: cscribe.c
	$(CC) $< -o $@ $(LIBS)
