CFLAGSEXTRA=
include ../makefile-lto.inc

PROJECT=CBZONE
PROJECTTYPE=GAMES
PRJSRC=$(wildcard *.c) $(wildcard *.cpp) $(wildcard *.s) $(wildcard *.h)
PRJOBJ=c_draw.o c_explode.o c_gpr.o c_graphics.o c_main.o c_move.o c_parseopts.o c_scores.o
CFLAGSEXTRA=-std=gnu17 -Wno-old-style-definition

include ../makefile-paws.inc
