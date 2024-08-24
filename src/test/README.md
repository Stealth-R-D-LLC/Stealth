# README for Stealth Tests


## CMake

When running `cmake`, specify paths to the GoogleTest,
Boost, and OpenSSL roots if using custom installations.
It is also possible to specify an external Stealth source directory.

The options for these directories are `-DGTEST_ROOT`, `-DBOOST_ROOT`,
`-DOPENSSL_ROOT` and `-DSTEALTH_SRC`, respectively.

The following sets all four paths:

```bash
cmake -DGTEST_ROOT=$DEPS/googletest/googletest-stealth \
      -DBOOST_ROOT=$OLD_DEPS/boost/boost-stealth \
      -DOPENSSL_ROOT=$DEPS/openssl/openssl-stealth \
      -DSTEALTH_SRC=$STEALTH_HOME/src ./
```

Specifying an external Stealth source directory allows pointing
to a reference implementation with which tests can be ran and
developed on a known working standard implementation. It will
also add the compiler definition `USING_EXTERNAL_STEALTH`,
setting it to `1`. This definition can be used in testing source.
It is equivalent to the preprocessor directive:

```cpp
#define USING_EXTERNAL_STEALTH=1
```

## Running the Testing Executable

### Debug Mode

The testing executable takes a `debug` flag (``--debug``, ``-d``).
The `debug` flag defaults to `false`, but
if set, testing will produce intermediate output representing
various data structures created during testing. Along with being
useful to debug code changes, the debugging output can
be used with a gold standard implementation to create more tests.

Assuming the testing executable is name `test-units`, all of the
following set the debug flag:

```bash
./test-units -d
./test-units -d 1
./test-units --debug true
```
