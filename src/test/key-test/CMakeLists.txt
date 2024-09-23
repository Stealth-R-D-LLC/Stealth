cmake_minimum_required(VERSION 3.0)

project(key-test)

set(target test-key)
add_executable(${target})

include(${CMAKE_SOURCE_DIR}/../CMakeCommon.cmake)

if(DEFINED ENV{LEGACY_PKEY})
   message(STATUS "Testing for legacy EC_KEY* pkey")
   add_compile_definitions(LEGACY_PKEY=1)
endif()

target_sources(${target} PRIVATE
    key-test.cpp
    ${STEALTH}/wallet/key.cpp
    ${STEALTH}/client/version.cpp
    ${STEALTH}/client/sync.cpp
    ${STEALTH}/blockchain/chainparams.cpp
    ${STEALTH}/primitives/valtype.cpp
    ${STEALTH}/util/util.cpp
    ${COMMON_CPP_SOURCES}
)

target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STEALTH}/wallet
    ${STEALTH}/network
    ${STEALTH}/util
    ${STEALTH}/blockchain
    ${STEALTH}/client
)

if(NOT DEFINED ENV{LEGACY_PKEY})
    set(C_SOURCES
        ${STEALTH}/crypto/core-hashes/memzero.c
        ${STEALTH}/crypto/core-hashes/ripemd160.c
        ${STEALTH}/crypto/core-hashes/sha2.c
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

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  target_link_options(${target} PRIVATE -lexecinfo)
endif()

target_link_libraries(${target}
    ${COMMON_LINK_LIBRARIES}
)
