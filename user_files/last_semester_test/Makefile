CC = gcc
DEBUG =
CFLAGS = -Wall -std=c99 $(DEBUG)
LDFLAGS=

TESTS = $(shell cat tests.list)

all: $(TESTS)

test%: test%.c test.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

.PHONY: clean
clean:
	-rm -f $(TESTS)
