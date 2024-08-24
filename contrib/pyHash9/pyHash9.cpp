// Copyright (c) 2016-2024 Stealth R&D LLC

#include <stddef.h>

#include "hashblock.h"

// forces python formatting to be Py_ssize_t
#define PY_SSIZE_T_CLEAN

#include <Python.h>

static const unsigned int HASHLEN = 32;

PyObject* digest(PyObject *self, PyObject *args)
{
    char* stringIn;
    Py_ssize_t size = 0;

    // can't parse args
    if (! PyArg_ParseTuple(args, "s#", &stringIn, &size)) {
      PyErr_SetString(PyExc_TypeError,
        "method expects a string(s)") ;
      return NULL ;
    }

    uint256 h = Hash9(stringIn, stringIn + size);

    char bytes[HASHLEN + 1];

    unsigned int i = 0;
    for (unsigned char *c = h.begin(); c != h.end(); ++c)
    {
        bytes[i] = (char)*c;
        i += 1;
    }
    bytes[HASHLEN] = 0x00;

    return PyBytes_FromStringAndSize(bytes, HASHLEN);
}

PyObject* hash9(PyObject *self, PyObject *args)
{
    char* stringIn;
    Py_ssize_t size = 0;

    // can't parse args
    if (! PyArg_ParseTuple(args, "s#", &stringIn, &size)) {
      PyErr_SetString(PyExc_TypeError,
        "method expects a string(s)") ;
      return NULL ;
    }

    uint256 hash = Hash9(stringIn, stringIn + size);

    return PyBytes_FromString(hash.ToString().c_str());
}

PyObject* hash(PyObject *self, PyObject *args)
{
    char* stringIn;
    Py_ssize_t size = 0;

    // can't parse args
    if (! PyArg_ParseTuple(args, "s#", &stringIn, &size)) {
      PyErr_SetString(PyExc_TypeError,
        "method expects a string(s)") ;
      return NULL ;
    }

    uint256 h = Hash9(stringIn, stringIn + size);

    char bytes[HASHLEN + 1];
    bytes[HASHLEN] = 0x00;

    // uint256 internal representation is reversed
    unsigned int i = HASHLEN - 1;
    for (unsigned char *c = h.begin(); c != h.end(); ++c)
    {
        bytes[i] = (char)*c;
        i -= 1;
    }

    return PyBytes_FromStringAndSize(bytes, HASHLEN);
}


static struct PyMethodDef pyHash9_methods[] =
{
    {"digest", digest, 1, "get the X13 digest as little endian bytes"},
    {"hash", hash, 1, "get the X13 digest as big endian bytes"},
    {"hash9", hash9, 1, "get the X13 digest as bytes in an ASCII encoded hex number"},
    {NULL, NULL}
};

static struct PyModuleDef pyHash9 =
{
    PyModuleDef_HEAD_INIT,
    "pyHash9",               // name of module
    "Calculate X13 hashes",  // module documentation, may be NULL
    -1,                      // size of per-interpreter state of the module,
                             //   or -1 if the module keeps state in global variables.
    pyHash9_methods
};

PyMODINIT_FUNC PyInit_pyHash9(void)
{
    return PyModule_Create(&pyHash9);
}
