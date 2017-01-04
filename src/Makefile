
OS := $(shell sh -c 'uname -s 2>/dev/null || echo unknow')

OPTIMIZATION?=-O2


RM=rm -rf
PREFIX?=/usr/local
INSTALL_BIN=$(PREFIX)/bin
INSTALL=install

STD=-std=c99 -pedantic
WARN=-Wall -W -Wno-missing-field-initializers
OPT=$(OPTIMIZATION)
DEBUG=-g -ggdb
RPS_DEBUG = $(RPS_DEBUG_OPEN)
GNU_SOURCE=-D_GNU_SOURCE #libuv need this declaration in linux platform.

DEPENDENCY_TARGETS=yaml libuv
LIBYAML=yaml-0.1.5
LIBUV=libuv-v1.9.1



FINAL_CFLAGS=$(WARN) $(OPT) $(DEBUG) $(RPS_DEBUG) $(CFLAGS)

FINAL_LDFLAGS=$(LDFLAGS) $(DEBUG)

ifeq ($(OS), Linux)
	FINAL_CFLAGS+=$(GNU_SOURCE) 
	FINAL_LIBS+= -lm -lrt -lpthread
else
ifeq ($(OS), Darwin)
	FINAL_CFLAGS+=$(STD) 
	FINAL_LIBS+= -lm
endif
endif

FINAL_CFLAGS+= -I../contrib/$(LIBYAML)/include 
FINAL_CFLAGS+= -I../contrib/$(LIBUV)/include 

FINAL_CFLAGS+= -Iproto

FINAL_LIBS+= ../contrib/$(LIBYAML)/src/.libs/libyaml.a
FINAL_LIBS+= ../contrib/$(LIBUV)/.libs/libuv.a

CC?=gcc
RPS_CC=$(CC) $(FINAL_CFLAGS)
RPS_LD=$(CC) $(FINAL_LDFLAGS)
RPS_INSTALL=$(INSTALL)


RPS_BIN=rps
RPS_OBJ=rps.o log.o config.o util.o array.o _string.o server.o 

%.o: %.c
	$(RPS_CC) -c $< -o $@ 

default: single


all: make-contrib $(RPS_BIN)
.PHONY: all

make-contrib:
	-(cd ../contrib && $(MAKE) $(DEPENDENCY_TARGETS)) 
.PHONY: make-contrib

$(RPS_BIN): $(RPS_OBJ) 
	$(RPS_LD) $^  -o $@ $(FINAL_LIBS)

single: $(RPS_BIN)
.PHONY: single

clean:
	$(RM) $(RPS_BIN) *.o *.gch \.*.swp *.i
.PHONY: clean

distclean: clean
	-(cd ../contrib && $(MAKE) distclean)

install: 
	@mkdir -p $(INSTALL_BIN)
	$(RPS_INSTALL) $(RPS_BIN) $(INSTALL_BIN)

noopt:
	$(MAKE) OPTIMIZATION="-O0"

debug:
	$(MAKE) RPS_DEBUG_OPEN="-DRPS_DEBUG_OPEN"
	
test:
	@echo $(FINAL_CFLAGS)
	@echo $(FINAL_LIBS)
.PHONY: test


