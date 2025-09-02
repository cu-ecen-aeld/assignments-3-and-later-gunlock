BINARY = aesdsocket

CC=$(CROSS_COMPILE)gcc
OPT=-O0
CFLAGS=-Wall -Wextra -g $(OPT)

OBJS = aesdsocket.o

.PHONY: all test test-memory clean

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

test:
	@"./$(BINARY)"

test-memory:
	@valgrind \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--verbose \
		--log-file=./valgrind-out.txt \
		./aesdsocket

clean:
	@rm -rf $(BINARY) $(OBJS) valgrind-out.txt
