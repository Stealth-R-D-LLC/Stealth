# Instructions to build on OS X

The newest version of this document is at

https://github.com/StealthSend/Stealth/wiki/Building-the-OS-X-Daemon


## Commands

* All shell commands in this document are for bash except when
  otherwise noted.
* Scripts are provided for both csh/tcsh and bash.


## System Library Paths

In later versions of OS X (e.g. Mojave and Catalina),
is may be necessary to specify paths to the system libraries:

```bash
CFLAGS="-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
CCFLAGS="-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
CXXFLAGS="-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
CPPFLAGS="-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
```


## Package Manager

The recommended package manager to build the daemon is
[Homebrew](https://https://brew.sh/).


## Libraries

Make sure you have installed the following packages with
linkable libraries, preferably using Homebrew. If you build from
scratch, make sure to enable building the static libraries when
running configure.

  * Berkeley DB 4.8: https://www.oracle.com/
  * Boost: http://www.boost.org/
  * LibEvent 2: http://libevent.org/
  * OpenSSL 1.1: https://www.openssl.org/
  * ZLib: http://www.zlib.net/

For Homebrew, the necessary packages are:

  * berkeley-db@4
  * boost
  * libevent
  * openssl@1.1

Once these packages are installed, you will have the correct
headers and static libraries.

Note that ZLib is already provided by the OS X system as of
OS X Catalina (version 10.15.3).


## Environment Variables

To use the homebrew packages, set the following
environment variables before make:

```bash
BDB_INCLUDE_PATH="/usr/local/opt/berkeley-db@4/include"
BDB_LIB_PATH="/usr/local/opt/berkeley-db@4/lib"
OPENSSL_INCLUDE_PATH="/usr/local/opt/openssl@1.1/include"
OPENSSL_LIB_PATH="/usr/local/opt/openssl@1.1/lib"
BOOST_INCLUDE_PATH="/usr/local/opt/boost/include"
BOOST_LIB_PATH="/usr/local/opt/boost/lib"
BOOST_LIB_SUFFIX="-mt"
```


## Make

To build first source the appropriate setup script:

**bash:**

```bash
source setup-build-osx.sh
```

**t/csh:**

```shell
source setup-build-osx.csh
```

**Note** that these scripts are designed for the recommendations above
(*i.e.* use homebrew and the suggested packages).

These scripts are located at:

  * https://raw.githubusercontent.com/StealthSend/Stealth/master/contrib/build-osx/setup-build-osx.sh
  * https://raw.githubusercontent.com/StealthSend/Stealth/master/contrib/build-osx/setup-build-osx.csh


The command to make (inside the "src" directory) would be

```bash
make -f makefile.osx
```
