APP := out
CC ?= gcc
CPPFLAGS ?= -DDEBUG
CFLAGS ?= -O0 -std=gnu99 -Wall -Wextra -g

.PHONY: all
all: $(APP)

$(APP): main.o systemcalls.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $^ -o $@

.PHONY: test
test: $(APP)
	# TODO

.PHONY: bear
bear:
	bear -- $(MAKE) clean
	bear -- $(MAKE)

.PHONY: clean
clean:
	@rm -rf *.o $(APP) 


