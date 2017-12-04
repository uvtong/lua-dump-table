
all:
	gcc -g -Wall -O3 -fPIC --shared lua-dump.c -I./lua-src/src -o dump.so