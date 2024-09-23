cmake_minimum_required(VERSION 3.0)

project(bip32-hash-test C CXX)

set(target test-bip32-hash)
add_executable(${target})

include(${CMAKE_SOURCE_DIR}/../CMakeCommon.cmake)

set(C_SOURCES
    ${STEALTH}/crypto/hashblock/aes_helper.c
    ${STEALTH}/crypto/hashblock/blake.c
    ${STEALTH}/crypto/hashblock/bmw.c
    ${STEALTH}/crypto/hashblock/cubehash.c
    ${STEALTH}/crypto/hashblock/echo.c
    ${STEALTH}/crypto/hashblock/fugue.c
    ${STEALTH}/crypto/hashblock/groestl.c
    ${STEALTH}/crypto/hashblock/hamsi.c
    ${STEALTH}/crypto/hashblock/hamsi_helper.c
    ${STEALTH}/crypto/hashblock/jh.c
    ${STEALTH}/crypto/hashblock/keccak.c
    ${STEALTH}/crypto/hashblock/luffa.c
    ${STEALTH}/crypto/hashblock/shavite.c
    ${STEALTH}/crypto/hashblock/simd.c
    ${STEALTH}/crypto/hashblock/skein.c
)

set(CPP_SOURCES
    ${STEALTH}/bip32/scrypt.cpp
    ${STEALTH}/bip32/pbkdf2.cpp
)

set(INCLUDE_DIRS
    ${STEALTH}/bip32
    ${STEALTH}/crypto/hashblock
)

if(DEFINED ENV{LEGACY_CORE_HASHES})
    message(STATUS "Testing with legacy hashes")
    add_compile_definitions(LEGACY_CORE_HASHES=1)
else()
    set(C_SOURCES
        ${C_SOURCES}
        ${STEALTH}/crypto/core-hashes/memzero.c
        ${STEALTH}/crypto/core-hashes/ripemd160.c
        ${STEALTH}/crypto/core-hashes/sha2.c
    )
    set(CPP_SOURCES
        ${CPP_SOURCES}
        ${STEALTH}/crypto/core-hashes/core-hashes.cpp
    )
    set(INCLUDE_DIRS
        ${INCLUDE_DIRS}
        ${STEALTH}/crypto/core-hashes
    )
endif()

set_source_files_properties(${C_SOURCES} PROPERTIES
    LANGUAGE C
)

target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${INCLUDE_DIRS}
)

target_sources(${target} PRIVATE
    bip32-hash-test.cpp
    ${C_SOURCES}
    ${CPP_SOURCES}
    ${COMMON_CPP_SOURCES}
)

target_link_libraries(${target}
    ${COMMON_LINK_LIBRARIES}
)
