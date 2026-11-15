
CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -O2 -pthread
AR = ar
ARFLAGS = rcs

all: client server

# Сборка статической библиотеки
libcommon.a: common.o
	$(AR) $(ARFLAGS) $@ $^

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c -o common.o

# Сборка клиента с библиотекой
client: client.c common.h libcommon.a
	$(CC) $(CFLAGS) client.c -L. -lcommon -o client

# Сборка сервера с библиотекой
server: server.c common.h libcommon.a
	$(CC) $(CFLAGS) server.c -L. -lcommon -o server

clean:
	rm -f client server common.o libcommon.a

.PHONY: all clean
