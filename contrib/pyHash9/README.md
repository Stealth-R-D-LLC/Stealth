# pyHash9

A python module that produces an X13 hash from an input string.


## Rationale

This module is useful for chain linearization and third party toolchains.


## Usage

    >>> import pyHash9
    >>> pyHash9.hash9("Why did they call it Hash9?")
    '3bddd71f63eb706ca44fd61743b5e23da7497b86db1164c3d2125432bd8d4c31'
    >>> pyHash9.hash("Why did they call it Hash9?").encode("hex")
    '3bddd71f63eb706ca44fd61743b5e23da7497b86db1164c3d2125432bd8d4c31'


## Building

### Linux

The version of python for Linux now defaults to Python 3
because official support for Python 2 has been discontinued.

It is necessary to install the Python 3 headers and libraries
and to link them to `/usr/local/`. These commands are system specific.
Ensure to link the `libpython*.a` built with PIC enabled. For example:

    % sudo apt-get install python3-dev libpython3-all-dev
    % sudo ln -s /usr/lib/python3.5/config-3.5m-x86_64-linux-gnu/libpython3.5m-pic.a /usr/local/lib/libpython.a
    % sudo ln -s /usr/include/python3.5m /usr/local/include/python

Then build:

    % cd pyHash9
    % make

### OS X

The pyHash9 module is meant to be built and used with Python 3.
However, as of OS X 10.15, OS X still ships with Python 2.7 as the default.
For this reason it is necessary to install the latest version of Python 3
from https://www.python.org/downloads/.

After installation, make the following links (these commands assume
the Python 3.8 was installed):

    % ln -s /Library/Frameworks/Python.framework/Versions/3.8/include/python3.8 /usr/local/include/python
    % ln -s /Library/Frameworks/Python.framework/Versions/3.8/Python /usr/local/lib/libpython3.8.dylib
    % ln -s /usr/local/lib/libpython3.8.dylib /usr/local/lib/libpython.dylib

Then build:

    % cd pyHash9
    % make$


## Installation

    % cp pyHash9.so ${PYTHONPATH}


## Copyright

Copyright (c) 2016-2020 Stealth R&D LLC
