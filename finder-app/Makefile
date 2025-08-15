BINARY = writer

CC=$(CROSS_COMPILE)gcc
OPT=-O0
CFLAGS=-Wall -Wextra -g $(OPT)

OBJECTS = writer.o

.PHONY: all test clean
	
all: $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

test:
	@./finder-test.sh

clean:
	@rm -rf $(BINARY) $(OBJECTS)
