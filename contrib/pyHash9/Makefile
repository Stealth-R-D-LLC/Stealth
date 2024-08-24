# Copyright (c) 2016-2024 Stealth R&D LLC

# Detect the operating system
UNAME_S := $(shell uname -s)

# specify PYINCDIR and PYLIBDIR if using non-system python
PYINCDIR = -I/usr/local/include/python
PYLIBDIR = -L/usr/local/lib

INCDIRS = $(PYINCDIR) -I../../src/crypto/hashblock -I../../src/primitives
CFLAGS = -g -Wall -fPIC
CXXFLAGS = -g -Wall -fPIC -std=c++11

VPATH = ../../src/crypto/hashblock

CC = gcc
CXX = g++
LDFLAGS = -shared -fPIC ${PYLIBDIR}

PROGS = pyHash9

OBJS = blake.o bmw.o groestl.o jh.o \
       keccak.o skein.o luffa.o \
       cubehash.o shavite.o simd.o \
       echo.o hamsi.o fugue.o \
       pyHash9.o

%.o: %.c
	${CC} ${CFLAGS} ${INCDIRS} -c $<

%.o: %.cpp
	${CXX} ${CXXFLAGS} ${INCDIRS} -c $<

%.o: %.cpp
	${CXX} ${CXXFLAGS} ${INCDIRS} -c $<

all: $(PROGS)

${PROGS}: %: %.o $(OBJS)
ifeq ($(UNAME_S),Linux)
	${CXX} ${CXXFLAGS} ${LDFLAGS} -o $@.so ${OBJS} -lpython-pic
else
	${CXX} ${CXXFLAGS} ${LDFLAGS} -o $@.so ${OBJS} -lpython
endif

clean:
	rm -f *.so *.o
