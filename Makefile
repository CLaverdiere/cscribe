CC=clang
LIBS=-lncurses -lsndfile -lportaudio -pthread
FLAGS=-Wall -g

cscribe: cscribe.c
	$(CC) $< -o $@ $(LIBS) $(FLAGS)
