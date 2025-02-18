#!/bin/bash
if [[ "$#" == "1" ]] && [[ "$1" == "clean" ]]; then
	rm rec rec_nolibc fact_rv64 exit exit-pie fib fib_nolc fib_nopf
elif [[ "$#" == "0" ]]; then
	gcc -Wall -Wpedantic rec.c -o rec
	gcc -Wall -Wpedantic -static -no-pie -fno-pie -nostdlib rec_nolibc.c -o rec_nolibc
	gcc -Wall -Wpedantic -static -no-pie -fno-pie -nostdlib fact_rv64.S rv64_runtime.S -o fact_rv64

	gcc -Wall -Wpedantic -static -nostdlib -no-pie -fno-pie -fno-pic exit.S -o exit
	gcc -Wall -Wpedantic -static -nostdlib  exit.S -o exit-pie

	gcc -Wall -Wpedantic -static fib.c -o fib
	gcc -Wall -Wpedantic -static fib_nopf.c -o fib_nopf
	gcc -Wall -Wpedantic -static -no-pie -fno-pie -nostdlib fib_nolc.c -o fib_nolc
fi
