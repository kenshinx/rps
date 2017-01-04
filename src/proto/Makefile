
OS := $(shell sh -c 'uname -s 2>/dev/null || echo unknow')

OPTIMIZATION?=-O2

RM=rm -rf

STD=-std=c99 -pedantic
WARN=-Wall -W -Wno-missing-field-initializers
OPT=$(OPTIMIZATION)
DEBUG=-g -ggdb
RPS_DEBUG = $(RPS_DEBUG_OPEN)


LIBUV=libuv-v1.9.1


FINAL_CFLAGS=$(WARN) $(OPT) $(DEBUG) $(RPS_DEBUG) $(CFLAGS)

ifeq ($(OS), Linux)
	FINAL_CFLAGS+=$(GNU_SOURCE) 
else
ifeq ($(OS), Darwin)
	FINAL_CFLAGS+=$(STD) 
endif
endif

FINAL_CFLAGS+= -I../

FINAL_CFLAGS+= -I../../contrib/$(LIBUV)/include 

CC?=gcc
AR?=ar 

RPS_CC=$(CC) $(FINAL_CFLAGS)
RPS_AR=$(AR)

PROTO_OBJS=s5.o http.o private.o

PROTO_LIB=proto.a

%.o: %.c
	$(RPS_CC) -c $< -o $@ 

$(PROTO_LIB): $(PROTO_OBJS)
	$(RPS_AR) rcs  $@ $^ 

default:$(PROTO_LIB)


clean:
	$(RM) *.o *.a
.PHONY: clean

noopt:
	$(MAKE) OPTIMIZATION="-O0"

debug:
	$(MAKE) RPS_DEBUG_OPEN="-DRPS_DEBUG_OPEN"
