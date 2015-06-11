CC=gcc
LIBS=-lncurses
FLAGS=-Wall -g

cscribe: cscribe.c
	$(CC) $< -o $@ $(LIBS) $(FLAGS)
