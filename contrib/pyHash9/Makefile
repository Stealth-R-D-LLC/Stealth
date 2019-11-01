# Copyright (c) 2016-2019 Stealth R&D LLC

# specify PYINCDIR and PYLIBDIR if using non-system python
PYINCDIR = -I/usr/local/include/python
PYLIBDIR = -L/usr/local/lib

INCDIRS = $(PYINCDIR) -I../../src
CFLAGS = -g -Wall -fPIC
CXXFLAGS = -g -Wall -fPIC -std=c++11

VPATH = ../../src

CC = gcc
CXX = g++
LDFLAGS = -shared -fpic ${PYLIBDIR}

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
	${CXX} ${CXXFLAGS} ${LDFLAGS} -o $@.so ${OBJS} -lpython

clean:
	rm -f *.so *.o
