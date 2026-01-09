
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?=-Wall -Wextra -g -O0
LDFLAGS ?= -pthread
VG_FLAGS = --leak-check=full \
					 --show-leak-kinds=all \
					 --track-origins=yes \
					 --verbose \
					 --log-file=./valgrind-out.txt

BINARY = aesdsocket
SRCS = aesdsocket.c list.c worker.c
HEADERS = list.h worker.h utility.h
OBJS = $(SRCS:.c=.o)

.PHONY: all test memcheck clean

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) $(OBJS) -o $(BINARY) $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

test:
	@"./$(BINARY)"

memcheck: $(BINARY)
	@valgrind $(VG_FLAGS) ./$(BINARY)

helgrind: $(BINARY)
	valgrind --tool=helgrind --history-level=approx ./$(BINARY)
	
clean:
	@rm -rf $(BINARY) $(OBJS) valgrind-out.txt

bear: clean
	bear -- make all
