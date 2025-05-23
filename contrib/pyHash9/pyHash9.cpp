// Copyright (c) 2016-2024 Stealth R&D LLC

#include <stddef.h>
#include <functional>
#include <string>

#include "hashblock.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>

static const unsigned int HASHLEN = 32;

static PyObject* process_input(
                   const char* funcName,
                   PyObject* args,
                   std::function<PyObject*(const char*, Py_ssize_t)> processor)
{
    PyObject* input;
    if (!PyArg_ParseTuple(args, "O", &input))
    {
        PyErr_SetString(PyExc_TypeError, "Failed to parse arguments");
        return NULL;
    }

    const char* buffer;
    Py_ssize_t size;

    if (PyBytes_Check(input))
    {
        if (PyBytes_AsStringAndSize(input,
                                    const_cast<char**>(&buffer),
                                    &size) == -1)
        {
            return NULL;
        }
    }
    else if (PyUnicode_Check(input))
    {
        PyObject* bytes = PyUnicode_AsEncodedString(input, "UTF-8", "strict");
        if (bytes == NULL)
        {
            return NULL;
        }
        if (PyBytes_AsStringAndSize(bytes,
                                    const_cast<char**>(&buffer),
                                    &size) == -1)
        {
            Py_DECREF(bytes);
            return NULL;
        }
        PyObject* result = processor(buffer, size);
        Py_DECREF(bytes);
        return result;
    }
    else
    {
        PyErr_Format(PyExc_TypeError,
                     "%s() argument must be str or bytes",
                     funcName);
        return NULL;
    }

    return processor(buffer, size);
}

static PyObject* digest(PyObject* self, PyObject* args)
{
    return process_input(
        "digest",
        args,
        [](const char* buffer, Py_ssize_t size)
        {
            uint256 h = Hash9(buffer, buffer + size);
            return PyBytes_FromStringAndSize((const char*) h.begin(), HASHLEN);
        });
}

static PyObject* hash9(PyObject* self, PyObject* args)
{
    return process_input("hash9",
                         args,
                         [](const char* buffer, Py_ssize_t size)
                         {
                             uint256 hash = Hash9(buffer, buffer + size);
                             std::string h = hash.ToString();
                             return PyBytes_FromStringAndSize(h.c_str(),
                                                              h.size());
                         });
}

static PyObject* hash(PyObject* self, PyObject* args)
{
    return process_input("hash",
                         args,
                         [](const char* buffer, Py_ssize_t size)
                         {
                             uint256 h = Hash9(buffer, buffer + size);
                             char bytes[HASHLEN];
                             for (unsigned int i = 0; i < HASHLEN; ++i)
                             {
                                 bytes[i] = h.begin()[HASHLEN - 1 - i];
                             }
                             return PyBytes_FromStringAndSize(bytes, HASHLEN);
                         });
}

static PyMethodDef pyHash9_methods[] =
{
          {"digest", digest, METH_VARARGS,
              "get the X13 digest as little endian bytes"},
          {"hash", hash, METH_VARARGS,
              "get the X13 digest as big endian bytes"},
          {"hash9", hash9, METH_VARARGS,
              "get the X13 digest as bytes in an ASCII encoded hex number"},
          {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pyHash9_module =
{
    PyModuleDef_HEAD_INIT,
    "pyHash9",               // name of module
    "Calculate X13 hashes",  // module documentation, may be NULL
    -1,                      // size of per-interpreter state of the module,
                             //    or -1 if the module keeps state
                             //    in global variables.
    pyHash9_methods
};

PyMODINIT_FUNC PyInit_pyHash9(void)
{
    return PyModule_Create(&pyHash9_module);
}
