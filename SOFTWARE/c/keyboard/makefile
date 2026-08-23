include ../makefile-lto.inc

PROJECT=KEYBOARD
PROJECTTYPE=TEST
PRJSRC=$(wildcard *.c) $(wildcard *.cpp) $(wildcard *.s) $(wildcard *.h)
PRJOBJ=keyboard.o
CFLAGSEXTRA=-fweb -fsched-spec-load -frename-registers

include ../makefile-paws.inc
