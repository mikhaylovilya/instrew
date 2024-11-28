#!/bin/bash
gcc -Wall -Wpedantic -static -no-pie rec.c -o rec
gcc -Wall -Wpedantic -static -no-pie -fno-pie -nostdlib rec_nolibc.c -o rec_nolibc
