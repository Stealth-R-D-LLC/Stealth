# Setup OS X Build (csh/tcsh script)

setenv CFLAGS "-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
setenv CCFLAGS "-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
setenv CXXFLAGS "-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
setenv CPPFLAGS "-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"

setenv BDB_INCLUDE_PATH "/usr/local/opt/berkeley-db@4/include"
setenv BDB_LIB_PATH "/usr/local/opt/berkeley-db@4/lib"
setenv OPENSSL_INCLUDE_PATH "/usr/local/opt/openssl@1.1/include"
setenv OPENSSL_LIB_PATH "/usr/local/opt/openssl@1.1/lib"
setenv BOOST_INCLUDE_PATH "/usr/local/opt/boost/include"
setenv BOOST_LIB_PATH "/usr/local/opt/boost/lib"
setenv BOOST_LIB_SUFFIX "-mt"
