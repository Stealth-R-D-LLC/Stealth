# pyHash9

A Python module that produces an X13 hash from an input string.


## Rationale

PyHash9 is useful for chain linearization and third party toolchains.

This module is a pure C/C++ implementation of X13, so it is much faster
than a Python implementation would be.


## Usage

PyHash9 offers three functions that operate on a single `bytes` argument,
called `data` herein:

* `digest`: returns the X13 digest of `data` as little endian `bytes`
* `hash`: returns the X13 digest of `data` as big endian `bytes`
* `hash9`: returns the X13 digest of `data` as `bytes` in a ASCII encoded hex number

The rule of thumb is that the endianness of `digest` reflects
other digest functions that return byte data, such as those from
hashlib.

However, the endianness of `hash` and `hash9` reflects string representations
of byte data as numbers, because string representations of numbers are
big endian in that the most significant digit is the earliest in the string.
The representation of cryptographic digests as numbers is ubiquitous in
the cryptocurrency field, with block and transaction identifiers being
just two examples.


### Examples

```python
>>> import pyHash9
>>> pyHash9.digest("Why did they call it Hash9?").hex()
'314c8dbd325412d2c36411db867b49a73de2b54317d64fa46c70eb631fd7dd3b'
>>> pyHash9.hash("Why did they call it Hash9?").hex()
'3bddd71f63eb706ca44fd61743b5e23da7497b86db1164c3d2125432bd8d4c31'
>>> pyHash9.hash9("Why did they call it Hash9?")
b'3bddd71f63eb706ca44fd61743b5e23da7497b86db1164c3d2125432bd8d4c31'
>>> pyHash9.hash9("Why did they call it Hash9?").decode("ASCII")
'3bddd71f63eb706ca44fd61743b5e23da7497b86db1164c3d2125432bd8d4c31'
```


## Build & Installation

### Building on Linux

The following instructions were made on Ubuntu 22.04 LTS,
and are likely different for other distributions,
especially those not based on Debian.

#### Setting up the Build Environment on Linux

The system version of Python for Linux now defaults to Python 3
because official support for Python 2 has been discontinued.

It is necessary to install the Python 3 headers and libraries
and to link them to `/usr/local/`. These commands are system specific.
Ensure to link the `libpython*.a` built with PIC enabled. For example,
on Ubuntu 22.04 LTS:

```bash
# Obtain the two part Python version (e.g. "3.10").
PYV=$(python --version 2>&1 | awk '{print $2}' | cut -d. -f1,2)
sudo apt-get update
sudo apt-get install python3-dev libpython3-all-dev
# Path to the correct version of libpython with PIC enabled.
PY_LIB="/usr/lib/python${PYV}/config-${PYV}-x86_64-linux-gnu/libpython${PYV}-pic.a"
sudo rm -f /usr/local/lib/libpython-pic.a
sudo rm -f /usr/local/include/python
sudo ln -s "${PY_LIB}" /usr/local/lib/libpython-pic.a
sudo ln -s "/usr/include/python${PYV}" /usr/local/include/python
```

#### Making on Linux

PyHash9 is meant to be built within the "contrib" directory inside
the "Stealth" source tree.

```bash
cd /path/to/Stealth/contrib/pyHash9
make
```

#### Installing on Linux

##### Personal Use

As a regular user, the best way to install pyHash9 is to add it to
your personal `site-packages` directory:

```bash
PYV=$(python --version 2>&1 | awk '{print $2}' | cut -d. -f1,2)
mkdir -p "${HOME}/.local/lib/python${PYV}/site-packages"
cp pyHash9.so "${HOME}/.local/lib/python${PYV}/site-packages"
```

##### Administrators

Linux administrators who want to install a system-wide pyHash9 available
to all users must jump through some hoops:

```bash
PYV=$(python --version 2>&1 | awk '{print $2}' | cut -d. -f1,2)
sudo mkdir -p "/usr/local/lib/python${PYV}/site-packages"
# Create a "site-packages.pth" file that points Python to pyHash9.so
echo "../site-packages" | sudo tee -a \
          "/usr/local/lib/python${PYV}/dist-packages/site-packages.pth"
sudo cp pyHash9.so "/usr/local/lib/python${PYV}/site-packages"
```


### Building on MacOS

Users should not typically modify the system Python bundled with MacOS.
For this reason it is necessary to install the latest version of Python 3
from https://www.python.org/downloads/. As of writing, the latest
Python 3 version is 3.12.

#### Setting up the Build Environment on MacOS

After installation, make the following links:

```bash
# Obtain the two part Python version (e.g. "3.12").
PYV=$(python --version 2>&1 | awk '{print $2}' | cut -d. -f1,2)
PY_FRAMEWORK="/Library/Frameworks/Python.framework/Versions/${PYV}"
PY_LIB_LINK="/usr/local/lib/libpython${PYV}.dylib"
# Remove any links for outdated Python versions.
rm -f /usr/local/include/python
rm -f "${PY_LIB_LINK}"
rm -f /usr/local/lib/libpython.dylib
ln -s "${PY_FRAMEWORK}/include/python${PYV}" /usr/local/include/python
ln -s "${PY_FRAMEWORK}/Python" "${PY_LIB_LINK}"
ln -s "${PY_LIB_LINK}" "/usr/local/lib/libpython.dylib"
```

#### Making on MacOS

PyHash9 is meant to be built within the "contrib" directory inside
the "Stealth" source tree.

```bash
cd /path/to/Stealth/contrib/pyHash9
make
```

#### Installing on MacOS

Installation for MacOS is similar to Linux, except the paths change.


##### Personal Use

As a regular user, the best way to install pyHash9 is to add it to
your personal `site-packages` directory:

```bash
PYV=$(python --version 2>&1 | awk '{print $2}' | cut -d. -f1,2)
mkdir -p "${HOME}/Library/Python/${PYV}/lib/python/site-packages"
cp pyHash9.so "${HOME}/Library/Python/${PYV}/lib/python/site-packages"
```

##### Administrators

Because the recommended way to use pyHash9 on MacOS is with a
non-system distribution of Python, the task of installing pyHash9
is slightly simpler on MacOS than Linux:

```bash
PYV=$(python --version 2>&1 | awk '{print $2}' | cut -d. -f1,2)
PY_FRAMEWORK="/Library/Frameworks/Python.framework/Versions/${PYV}"
sudo cp pyHash9.so "${PY_FRAMEWORK}/lib/python${PYV}/site-packages"
```


## Copyright

Copyright (c) 2016-2024 Stealth R&D LLC
