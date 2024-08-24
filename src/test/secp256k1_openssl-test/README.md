# Readme for Testing: `secp256k1_openssl-test`

## Coverage

* `bip32/secp256k1_openssl.cpp`
* `bip32/secp256k1_openssl.h`

## Usage

Testing is built with `cmake`, and the testing executable
is `test-secp256k1_openssl-test`.

```
cmake ./
make
test-secp256k1_openssl
```

### RFC6979

RFC6979 was part of the original implementation by Ciphrex,
but is not used herein, nor has its necessity been demonstrated.
RFC6979 has therefore been removed indefinitely, but the testing
functionality remains as a conditional compilation, for comparison against the
original Ciphrex implementation. Turn on RFC6979 testing during `cmake`
with the environment variable `TEST_RFC6979`:

```sh
TEST_RFC6979=1 cmake -DSTEALTH_SRC=$STEALTH_HOME/src
```

Notice that if RFC6979 testing is desired, a source tree implementing
it should be specified with the `-DSTEALTH_SRC` flag.

## More Info

Please see [../README.md](../README.md) for how to use
custom environments and special options.
