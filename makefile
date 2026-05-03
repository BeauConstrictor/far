CC = gcc
CFLAGS = -Wall -Wextra -g

all: build/far

build/:
	mkdir -p build

build/main.o: src/main.c src/parser.h | build/
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o

build/parser.o: src/parser.c src/parser.h | build/
	$(CC) $(CFLAGS) -c src/parser.c -o build/parser.o

build/far: build/main.o build/parser.o
	$(CC) build/main.o build/parser.o -o build/far

.PHONY: all run dbg clean

run: build/far
	./build/far -p . -o test.far
	rm test.far

dbg: build/far
	gdb --args ./build/far -l example.far

clean:
	rm -rf build
