cmake_minimum_required(VERSION 3.0)

project(secp256k1_openssl-test)

set(target test-secp256k1_openssl)
add_executable(${target})

include(${CMAKE_SOURCE_DIR}/../CMakeCommon.cmake)

if(DEFINED ENV{LEGACY_CORE_HASHES})
   message(STATUS "Testing with legacy hashes")
   add_compile_definitions(LEGACY_CORE_HASHES=1)
endif()

if(DEFINED ENV{TEST_RFC6979})
   message(STATUS "Testing rfc6979")
   add_compile_definitions(DO_RFC6979_TESTS=1)
endif()


target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STEALTH}/bip32
    ${STEALTH}/crypto/hashblock
)

target_sources(${target} PRIVATE
    secp256k1_openssl-test.cpp
    ${STEALTH}/bip32/secp256k1_openssl.cpp
    ${COMMON_CPP_SOURCES}
)

if(NOT DEFINED ENV{LEGACY_CORE_HASHES})
    set(C_SOURCES
        ${STEALTH}/crypto/core-hashes/ripemd160.c
        ${STEALTH}/crypto/core-hashes/sha2.c
        ${STEALTH}/crypto/core-hashes/memzero.c
    )
    target_sources(${target} PRIVATE
        ${C_SOURCES}
        ${STEALTH}/crypto/core-hashes/core-hashes.cpp
    )
    target_include_directories(${target} PRIVATE
        ${STEALTH}/crypto/core-hashes
    )
    set_source_files_properties(${C_SOURCES} PROPERTIES
        LANGUAGE C
    )
endif()

target_link_libraries(${target}
    ${COMMON_LINK_LIBRARIES}
)
