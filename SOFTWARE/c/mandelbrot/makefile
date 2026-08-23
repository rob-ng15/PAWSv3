include ../makefile-lto.inc

PROJECT=MANDEL
PROJECTTYPE=DEMO
PRJSRC=$(wildcard *.c) $(wildcard *.cpp) $(wildcard *.s) $(wildcard *.h)
PRJOBJ=mandelbrot.o
CFLAGSEXTRA=-fweb -fsched-spec-load -frename-registers

include ../makefile-paws.inc
