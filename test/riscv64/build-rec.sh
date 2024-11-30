#!/bin/bash
gcc -Wall -Wpedantic -static -no-pie rec.c -o rec
gcc -Wall -Wpedantic -static -no-pie -fno-pie -nostdlib rec_nolibc.c -o rec_nolibc

gcc -Wall -Wpedantic -static -nostdlib -no-pie -fno-pie -fno-pic exit.S -o exit
gcc -Wall -Wpedantic -static -nostdlib  exit.S -o exit-pie
