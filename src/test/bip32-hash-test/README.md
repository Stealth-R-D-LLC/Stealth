# Readme for Testing: `bip32-hash-test`

## Coverage

* `bip32/hash.h`
* `bip32/pbkdf2.cpp`
* `bip32/pbkdf2.h`
* `bip32/scrypt.cpp`
* `bip32/scrypt.h`

## Usage

Testing is built with `cmake`, and the testing executable
is `test-bip32-hash`.

```
cmake ./
make
test-bip32-hash
```

## More Info

Please see [../README.md](../README.md) for how to use
custom environments and special options.
