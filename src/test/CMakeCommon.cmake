# common-cmake-configuration.cmake

cmake_minimum_required(VERSION 3.0)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-std=c11>
)
endif()


set(COMMON_DIR "${CMAKE_SOURCE_DIR}/..")

######################################################################
# Stealth Sources
######################################################################
# This path is relative to the enclosed "*-test" directores.
set(STEALTH_DEFAULT_SRC "../..")

if(DEFINED STEALTH_SRC)
   # get_filename_component(dflt "${CMAKE_SOURCE_DIR}/${STEALTH_DEFAULT_SRC}" REALPATH)
   # get_filename_component(user "${CMAKE_SOURCE_DIR}/${STEALTH_SRC}" REALPATH)
   get_filename_component(dflt "${STEALTH_DEFAULT_SRC}" REALPATH)
   get_filename_component(user "${STEALTH_SRC}" REALPATH)
   message(STATUS "Default: <${dflt}>")
   message(STATUS "   User: <${user}>")
   if(user STREQUAL dflt)
       message(STATUS "Specified default Stealth root: ${STEALTH_SRC}")
   else()
       message(STATUS "Using external Stealth root: ${STEALTH_SRC}")
       add_compile_definitions(USING_EXTERNAL_STEALTH=1)
       set(EXTERNAL_STEALTH TRUE)
   endif()
   set(STEALTH "${STEALTH_SRC}")
else()
   message(STATUS "Using default Stealth root: ${STEALTH_SRC}")
   set(STEALTH "${STEALTH_DEFAULT_SRC}")
endif()

######################################################################
# OpenSSL
######################################################################
if(DEFINED OPENSSL_ROOT)
    message(STATUS "Using nonstandard OpenSSL root: ${OPENSSL_ROOT}")
    set(OPENSSL_ROOT_DIR ${OPENSSL_ROOT})
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/include)
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_ROOT_DIR}/lib/libcrypto.a)
    set(OPENSSL_SSL_LIBRARY ${OPENSSL_ROOT_DIR}/lib/libssl.a)
    set(OpenSSL_DIR ${OPENSSL_ROOT_DIR})
endif()

find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL found: ${OPENSSL_FOUND}")
message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL crypto library: ${OPENSSL_CRYPTO_LIBRARY}")
message(STATUS "OpenSSL ssl library: ${OPENSSL_SSL_LIBRARY}")
message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")

######################################################################
# GTest
######################################################################
if(DEFINED GTEST_ROOT)
    set(GTEST_ROOT_DIR ${GTEST_ROOT})
    message(STATUS "Using custom GTest root: ${GTEST_ROOT_DIR}")
    set(GTEST_INCLUDE_DIR "${GTEST_ROOT_DIR}/include")
    set(GTEST_LIBRARY "${GTEST_ROOT_DIR}/lib/libgtest.a")
    set(GTEST_MAIN_LIBRARY "${GTEST_ROOT_DIR}/lib/libgtest_main.a")
    set(GTest_DIR "${GTEST_ROOT_DIR}/lib/cmake")
endif()
find_package(GTest REQUIRED)
if(GTest_FOUND)
    message(STATUS "GTest found successfully")
    message(STATUS "GTest version: ${GTest_VERSION}")
    message(STATUS "GTest include dir: ${GTest_INCLUDE_DIRS}")
    message(STATUS "GTest libraries: ${GTest_LIBRARIES}")
else()
    message(FATAL_ERROR "GTest not found. Please specify the GTest "
                           "installation directory using "
                           "-DGTEST_ROOT=/path/to/gtest")
endif()

######################################################################
# Boost
######################################################################
if(DEFINED BOOST_ROOT)
    set(Boost_NO_SYSTEM_PATHS TRUE)
    set(Boost_NO_BOOST_CMAKE TRUE)
    message(STATUS "Using custom Boost root: ${BOOST_ROOT}")
    set(Boost_USE_STATIC_LIBS ON)
    set(BOOST_INCLUDEDIR "${BOOST_ROOT}/include")
    set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib")
    set(Boost_DIR "${BOOST_ROOT}/lib/cmake")
endif()
find_package(Boost REQUIRED system filesystem program_options thread chrono atomic)
if(Boost_FOUND)
    message(STATUS "Boost found successfully")
    message(STATUS "Boost version: ${Boost_VERSION}")
    message(STATUS "Boost include dir: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
else()
    message(FATAL_ERROR "Boost not found. Please specify the Boost "
                           "installation directory using "
                           "-DBOOST_ROOT=/path/to/boost")
endif()


######################################################################
# Paths
######################################################################
include_directories(
    ${OPENSSL_INCLUDE_DIR}
    ${GTEST_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${COMMON_DIR}
    ${STEALTH}/util
    ${STEALTH}/primitives
)

set(COMMON_LINK_LIBRARIES
    ${OPENSSL_CRYPTO_LIBRARY}
    ${OPENSSL_SSL_LIBRARY}
    GTest::gtest
    GTest::gtest_main
    Boost::atomic
    Boost::chrono
    Boost::system
    Boost::filesystem
    Boost::program_options
    Boost::thread
)

set(COMMON_CPP_SOURCES
    ${COMMON_DIR}/test-utils.cpp
    ${STEALTH}/primitives/valtype.cpp
)
