CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
SRCS = src/tile.c
TEST_SRCS = tests/test_tile.c

all: lib

lib: $(SRCS) include/tile.h
	$(CC) $(CFLAGS) -c src/tile.c -o build/tile.o
	ar rcs build/libplato-tile-spec.a build/tile.o

test: $(SRCS) $(TEST_SRCS) include/tile.h
	mkdir -p build
	$(CC) $(CFLAGS) -o build/test_tile $(SRCS) $(TEST_SRCS)
	./build/test_tile

clean:
	rm -rf build

.PHONY: all lib test clean
