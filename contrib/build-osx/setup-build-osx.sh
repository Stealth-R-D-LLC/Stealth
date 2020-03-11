# Setup OS X Build (bash script)

CFLAGS="-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
CCFLAGS="-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
CXXFLAGS="-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
CPPFLAGS="-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"

BDB_INCLUDE_PATH="/usr/local/opt/berkeley-db@4/include"
BDB_LIB_PATH="/usr/local/opt/berkeley-db@4/lib"
OPENSSL_INCLUDE_PATH="/usr/local/opt/openssl@1.1/include"
OPENSSL_LIB_PATH="/usr/local/opt/openssl@1.1/lib"
BOOST_INCLUDE_PATH="/usr/local/opt/boost/include"
BOOST_LIB_PATH="/usr/local/opt/boost/lib"
BOOST_LIB_SUFFIX="-mt"
