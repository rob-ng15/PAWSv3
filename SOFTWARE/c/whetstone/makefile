CFLAGSEXTRA=
include ../makefile-lto.inc

PROJECT=W-STONE
PROJECTTYPE=TEST
PRJSRC=$(wildcard *.c) $(wildcard *.cpp) $(wildcard *.s) $(wildcard *.h)
PRJOBJ=whetstone.o
CFLAGSEXTRA=-fweb -fsched-spec-load -frename-registers

include ../makefile-paws.inc
