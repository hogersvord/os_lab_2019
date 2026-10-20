CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -I.

all: parallel_min_max process_memory parallel_sum

parallel_min_max: parallel_min_max.c utils.o find_min_max.o
	$(CC) $(CFLAGS) -o parallel_min_max parallel_min_max.c utils.o find_min_max.o

process_memory: process_memory.c
	$(CC) $(CFLAGS) -o process_memory process_memory.c

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c -o utils.o

find_min_max.o: find_min_max.c find_min_max.h utils.h
	$(CC) $(CFLAGS) -c find_min_max.c -o find_min_max.o
parallel_sum: parallel_sum.c libsum.a utils.o sum.h utils.h
	$(CC) $(CFLAGS) -pthread -o parallel_sum parallel_sum.c utils.o -L. -lsum

sum.o: sum.c sum.h
	$(CC) $(CFLAGS) -c sum.c -o sum.o

libsum.a: sum.o
	ar rcs libsum.a sum.o
clean:
	rm -f utils.o find_min_max.o parallel_min_max process_memory

.PHONY: all clean
