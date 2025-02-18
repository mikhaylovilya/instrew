CC = gcc 
.PHONY: all clean c
all: 
	$(CC) -Wall -Wpedantic -static rec.c -o rec
	$(CC) -Wall -Wpedantic -static -no-pie -fno-pie -nostdlib rec_nolibc.c -o rec_nolibc
	$(CC) -Wall -Wpedantic -static -no-pie -fno-pie -nostdlib fact_rv64.S rv64_runtime.S -o fact_rv64
# 
	$(CC) -Wall -Wpedantic -static -nostdlib -no-pie -fno-pie -fno-pic exit.S -o exit
	$(CC) -Wall -Wpedantic -static -nostdlib  exit.S -o exit-pie
#
	$(CC) -Wall -Wpedantic -static fib.c -o fib
	$(CC) -Wall -Wpedantic -static fib_nopf.c -o fib_nopf
	$(CC) -Wall -Wpedantic -static -no-pie -fno-pie -nostdlib fib_nolc.c -o fib_nolc
c: clean
clean:
	rm rec rec_nolibc fact_rv64 exit exit-pie fib fib_nolc fib_nopf

