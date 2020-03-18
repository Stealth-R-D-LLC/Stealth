# Setup OS X Build (bash script)

ISYSROOT="-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

CFLAGS="${ISYSROOT}"
CCFLAGS="${ISYSROOT}"
CXXFLAGS="${ISYSROOT}"
CPPFLAGS="${ISYSROOT}"

BDB_INCLUDE_PATH="/usr/local/opt/berkeley-db@4/include"
BDB_LIB_PATH="/usr/local/opt/berkeley-db@4/lib"
OPENSSL_INCLUDE_PATH="/usr/local/opt/openssl@1.1/include"
OPENSSL_LIB_PATH="/usr/local/opt/openssl@1.1/lib"
BOOST_INCLUDE_PATH="/usr/local/opt/boost/include"
BOOST_LIB_PATH="/usr/local/opt/boost/lib"
BOOST_LIB_SUFFIX="-mt"

echo "Successfully setup OS X build environment."
