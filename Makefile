
all:
	gcc -g -Wall -fPIC --shared lua-dump.c -I./lua-src/src -o dump.so