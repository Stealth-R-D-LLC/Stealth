// Copyright (c) 2016-2018, Stealth R&D LLC

#include <stddef.h>

#include "hashblock.h"

#include <Python.h>

PyObject* hash9(PyObject *self, PyObject *args)
{
    char* stringIn;
    int size = 0;

    /* can't parse args */
    if (! PyArg_ParseTuple(args, "s#", &stringIn, &size)) {
      PyErr_SetString(PyExc_TypeError,
        "method expects a string(s)") ;
      return NULL ;
    }

    uint256 hash = Hash9(stringIn, stringIn + size);

    return PyString_FromString(hash.ToString().c_str());
}

PyObject* hash(PyObject *self, PyObject *args)
{
    char* stringIn;
    int size = 0;

    /* can't parse args */
    if (! PyArg_ParseTuple(args, "s#", &stringIn, &size)) {
      PyErr_SetString(PyExc_TypeError,
        "method expects a string(s)") ;
      return NULL ;
    }

    uint256 h = Hash9(stringIn, stringIn + size);

    std::string b;
    for (unsigned char *c = h.end(); c != h.begin(); --c)
    {
        b.push_back(*(c - 1));
    }

    return PyString_FromString(b.c_str());
}


static struct PyMethodDef pyHash9_methods[] =
{
    {"hash9", hash9, 1, "get the X13 hash as hex"},
    {"hash", hash, 1, "get the X13 hash"},
    {NULL, NULL}
};


extern "C" void initpyHash9(void)
{
    (void) Py_InitModule("pyHash9", pyHash9_methods);
}
