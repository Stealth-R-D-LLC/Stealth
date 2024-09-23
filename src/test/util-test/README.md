# Readme for Testing: `util-test`

## Coverage

* `util/util.cpp` (partial)
* `util/util.h` (partial)

## Usage

Testing is built with `cmake`, and the testing executable
is `test-core-hashes`.

```
cmake ./
make
test-core-hashes
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

## More Info

Please see [../README.md](../README.md) for how to use
custom environments and special options.
