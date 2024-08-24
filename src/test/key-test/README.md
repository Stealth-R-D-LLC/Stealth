# Readme for Testing: `key-test`

## Coverage

* `wallet/key.cpp`
* `wallet/key.h`

## Usage

Testing is built with `cmake`, and the testing executable
is `test-key`.

```
cmake ./
make
test-key
```

## Legacy Key Code

To ensure the testing suite tests properly, it is possible
to run the tests assuming the legacy definition of `pkey`
within the class `CKey`, instead of the latest definition of `pkey`:

```bash
LEGACY_PKEY=1 cmake -DSTEALTH_SRC=$STEALTH_HOME/src ./
```

Notice that if assuming a legacy `pkey`, it would be necessary to
point `cmake` to a source tree implementing it using the `-DSTEALTH_SRC`
command line option.


## More Info

Please see [../README.md](../README.md) for how to use
custom environments and special options.
