
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
RPG_DEBUG = $(RPG_DEBUG_OPEN)

DEPENDENCY_TARGETS=yaml libuv
LIBYAML=yaml-0.1.5
LIBUV=libuv-v1.9.1

FINAL_CFLAGS=$(STD) $(WARN) $(OPT) $(DEBUG) $(RPG_DEBUG) $(CFLAGS)
FINAL_CFLAGS+= -I../contrib/$(LIBYAML)/include 
FINAL_CFLAGS+= -I../contrib/$(LIBUV)/include 

FINAL_LDFLAGS=$(LDFLAGS) $(DEBUG)

ifeq ($(OS), Linux)
	FINAL_LIBS+= -lm -lrt -lpthread
else
ifeq ($(OS), Darwin)
	FINAL_LIBS+= -lm
endif
endif

FINAL_LIBS+= ../contrib/$(LIBYAML)/src/.libs/libyaml.a
FINAL_LIBS+= ../contrib/$(LIBUV)/.libs/libuv.a

CC?=gcc
RPG_CC=$(CC) $(FINAL_CFLAGS)
RPG_LD=$(CC) $(FINAL_LDFLAGS)
RPG_INSTALL=$(INSTALL)

RPG_BIN=rpg
RPG_OBJ=rpg.o log.o config.o util.o array.o string.o server.o

%.o: %.c
	$(RPG_CC) -c $<

default: single


all: make-contrib $(RPG_BIN)
.PHONY: all

make-contrib:
	-(cd ../contrib && $(MAKE) $(DEPENDENCY_TARGETS)) 
.PHONY: make-contrib

$(RPG_BIN): $(RPG_OBJ) 
	$(RPG_LD) $(FINAL_LIBS) $^  -o $@ 

single: $(RPG_BIN)
.PHONY: single

clean:
	$(RM) $(RPG_BIN) *.o *.gch \.*.swp *.i
.PHONY: clean

distclean: clean
	-(cd ../contrib && $(MAKE) distclean)

install: 
	@mkdir -p $(INSTALL_BIN)
	$(RPG_INSTALL) $(RPG_BIN) $(INSTALL_BIN)

noopt:
	$(MAKE) OPTIMIZATION="-O0"

debug:
	$(MAKE) RPG_DEBUG_OPEN="-DRPG_DEBUG_OPEN"
	
test:
	@echo $(FINAL_CFLAGS)
	@echo $(FINAL_LIBS)
.PHONY: test


