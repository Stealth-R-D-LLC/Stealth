# Copyright (c) 2016-2024 Stealth R&D LLC

UNAME_S := $(shell uname -s)

DEFAULT_PYINCDIR := /usr/local/include/python
PYINCDIR ?= $(DEFAULT_PYINCDIR)
ifeq ($(PYINCDIR),$(DEFAULT_PYINCDIR))
    $(info Using default PYINCDIR: $(PYINCDIR))
endif

INCDIRS = -I$(PYINCDIR) -I../../src/crypto/hashblock -I../../src/primitives
CFLAGS = -g -Wall -fPIC
CXXFLAGS = -g -Wall -fPIC -std=c++11


PYTHON_H_SYMBOLS = _PyBytes_FromStringAndSize \
                   _PyErr_SetString \
                   _PyExc_TypeError \
                   _PyModule_Create2 \
                   __PyArg_ParseTuple_SizeT \
                   _PyBytes_AsStringAndSize \
                   _PyErr_Format \
                   _PyUnicode_AsEncodedString \
                   _PyUnicode_FromString \
                   __Py_Dealloc

IGNORE_SYMBOLS := $(foreach sym,$(PYTHON_H_SYMBOLS), -Wl,-U,$(sym))

VPATH = ../../src/crypto/hashblock

CC = gcc
CXX = g++
LDFLAGS = -shared -fPIC

PROGS = pyHash9

OBJS = blake.o bmw.o groestl.o jh.o \
       keccak.o skein.o luffa.o \
       cubehash.o shavite.o simd.o \
       echo.o hamsi.o fugue.o \
       pyHash9.o

%.o: %.c
	$(CC) $(CFLAGS) $(INCDIRS) -c $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCDIRS) -c $<

all: $(PROGS)

$(PROGS): %: %.o $(OBJS)
ifeq ($(UNAME_S),Darwin)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(IGNORE_SYMBOLS) -o $@.so $(OBJS)
else
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@.so $(OBJS)
endif

clean:
	rm -f *.so *.o
