
Compilation instructions:

vasm (68k):
$ make CPU=m68k SYNTAX=mot -e CC=gcc

vasm (6502):
$ make CPU=6502 SYNTAX=oldstyle -e CC=gcc

vasm (z80):
$ make CPU=z80 SYNTAX=oldstyle -e CC=gcc
$ make CPU=z80 SYNTAX=std -e CC=gcc

vasm (ppc):
$ make CPU=ppc SYNTAX=oldstyle -e CC=gcc

