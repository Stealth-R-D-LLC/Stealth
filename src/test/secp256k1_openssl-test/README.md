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

## Legacy Core Hashes

To ensure the testing suite tests properly, it is possible
to run the tests assuming the legacy definitions of core hashes
sha1 and ripemd160, which use OpenSSL. This flag allows testing
of Stealth versions preceeding 3.2 as an external code base.

```bash
LEGACY_CORE_HASHES=1 cmake -DSTEALTH_SRC=$STEALTH_HOME/src ./
```

Notice that if assuming legacy hashes, it would be necessary to
point `cmake` to a source tree implementing it using the `-DSTEALTH_SRC`
command line option.

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
