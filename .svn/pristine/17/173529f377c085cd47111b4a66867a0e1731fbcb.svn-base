TEMP1 = $(shell echo `pwd`)
TEMP2 = $(shell echo `dirname ${TEMP1}`)
TEMP3 = $(shell echo `dirname ${TEMP2}`)

PROJ = rps

SRC			= $(PROJ).tar.gz
PKG 		= $(wildcard *.spec)
TEMP_DIR 	= /home/$(shell whoami)/tmp
#ROOT_DIR 	= $(shell PWD=$$(pwd); echo $${PWD%%/$(PROJ)*}/$(PROJ))
ROOT_DIR 	= $(TEMP2)
SVNVERSION  = 1.0.$(shell svnversion)

