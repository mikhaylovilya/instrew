#!/bin/bash
gcc -fPIC -march=rv64i2p1 -mabi=lp64 -c sc.c -o sc.o
gcc -fPIC -march=rv64i2p1 -mabi=lp64 -Wl,-e S0_1010c func_1010c.elf sc.o
