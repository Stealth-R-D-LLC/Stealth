# README for Boost Tests

## Update 2024-08-19

These [Boost](https://www.boost.org/) tests have been out of
use for a long time, but most are still useful tests.

However, Stealth will start using
[GoogleTest](https://github.com/google/googletest), which is
easier to use and provides better debugging output.

The current plan is to write new tests with GoogleTest
and convert the existing Boost tests, with updates, to
GoogleTest as well.

The original README information follows.

## Original README

### Rationale

The sources in this directory are unit test cases.  Boost includes a
unit testing framework, and since bitcoin already uses boost, it makes
sense to simply use this framework rather than require developers to
configure some other framework (we want as few impediments to creating
unit tests as possible).

### Creating New Tests

The build system is setup to compile an executable called "test_bitcoin"
that runs all of the unit tests.  The main source file is called
test_bitcoin.cpp, which simply includes other files that contain the
actual unit tests (outside of a couple required preprocessor
directives).  The pattern is to create one test file for each class or
source file for which you want to create unit tests.  The file naming
convention is "<source_filename>_tests.cpp" and such files should wrap
their tests in a test suite called "<source_filename>_tests".  For an
examples of this pattern, examine uint160_tests.cpp and
uint256_tests.cpp.

For further reading, I found the following website to be helpful in
explaining how the boost unit test framework works:

http://www.alittlemadness.com/2009/03/31/c-unit-testing-with-boosttest/
