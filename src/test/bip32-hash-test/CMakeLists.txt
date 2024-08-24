cmake_minimum_required(VERSION 3.30)

project(bip32-hash-test C CXX)

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

add_executable(test-bip32-hash)

target_sources(test-bip32-hash PRIVATE
    bip32-hash-test.cpp
    ${STEALTH}/bip32/scrypt.cpp
    ${STEALTH}/bip32/pbkdf2.cpp
    ${C_SOURCES}
    ${COMMON_CPP_SOURCES}
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test-bip32-hash PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-std=c11>
)
endif()

set_source_files_properties(${C_SOURCES} PROPERTIES
    LANGUAGE C
)

target_include_directories(test-bip32-hash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STEALTH}/bip32
    ${STEALTH}/crypto
)

target_link_libraries(test-bip32-hash
    ${COMMON_LINK_LIBRARIES}
)
