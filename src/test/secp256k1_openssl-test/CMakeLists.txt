cmake_minimum_required(VERSION 3.30)

project(secp256k1_openssl-test)

include(${CMAKE_SOURCE_DIR}/../CMakeCommon.cmake)

if(DEFINED ENV{TEST_RFC6979})
   message(STATUS "Testing rfc6979")
   add_compile_definitions(DO_RFC6979_TESTS=1)
endif()

add_executable(test-secp256k1_openssl)

target_sources(test-secp256k1_openssl PRIVATE
    secp256k1_openssl-test.cpp
    ${STEALTH}/bip32/secp256k1_openssl.cpp
    ${COMMON_CPP_SOURCES}
)

target_include_directories(test-secp256k1_openssl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STEALTH}/bip32
    ${STEALTH}/crypto
)

target_link_libraries(test-secp256k1_openssl
    ${COMMON_LINK_LIBRARIES}
)
