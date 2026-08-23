include ../makefile-lto.inc

PROJECT=MY_BASIC
PROJECTTYPE=COMPUTER
PRJSRC=$(wildcard shell/*.c) $(wildcard core/*.c) $(wildcard core/*.h)
PRJOBJ=shell/main.o core/my_basic.o
CFLAGSEXTRA=-fweb -fsched-spec-load -frename-registers

include ../makefile-paws.inc
