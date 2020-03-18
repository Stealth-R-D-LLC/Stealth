# Setup OS X Build (csh/tcsh script)

set ISYSROOT = "-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

setenv CFLAGS "${ISYSROOT}"
setenv CCFLAGS "${ISYSROOT}"
setenv CXXFLAGS "${ISYSROOT}"
setenv CPPFLAGS "${ISYSROOT}"

setenv BDB_INCLUDE_PATH "/usr/local/opt/berkeley-db@4/include"
setenv BDB_LIB_PATH "/usr/local/opt/berkeley-db@4/lib"
setenv OPENSSL_INCLUDE_PATH "/usr/local/opt/openssl@1.1/include"
setenv OPENSSL_LIB_PATH "/usr/local/opt/openssl@1.1/lib"
setenv BOOST_INCLUDE_PATH "/usr/local/opt/boost/include"
setenv BOOST_LIB_PATH "/usr/local/opt/boost/lib"
setenv BOOST_LIB_SUFFIX "-mt"

echo "Successfully setup OS X build environment."
