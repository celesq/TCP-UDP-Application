CC = gcc
CFLAGS = -Wall -g
DEPS = common.h helpers.h

all: server subscriber

server: server.c common.o
	pkill server 2>/dev/null || true
	$(CC) $(CFLAGS) -o server server.c common.o -lm

subscriber: client.c common.o
	$(CC) $(CFLAGS) -o subscriber client.c common.o -lm

common.o: common.c $(DEPS)
	$(CC) $(CFLAGS) -c common.c

clean:
	pkill server 2>/dev/null || true
	rm -f server subscriber *.o

