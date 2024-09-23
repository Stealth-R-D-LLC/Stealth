cmake_minimum_required(VERSION 3.0)

project(util-test)

set(target test-util)
add_executable(${target})

include(${CMAKE_SOURCE_DIR}/../CMakeCommon.cmake)

if(DEFINED ENV{LEGACY_CORE_HASHES})
   message(STATUS "Testing with legacy hashes")
   add_compile_definitions(LEGACY_CORE_HASHES=1)
endif()

target_include_directories(test-util PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STEALTH}/util
    ${STEALTH}/client
    ${STEALTH}/crypto/hashblock
    ${STEALTH}/blockchain
)

target_sources(test-util PRIVATE
    util-test.cpp
    ${STEALTH}/util/util.cpp
    ${STEALTH}/client/version.cpp
    ${STEALTH}/blockchain/chainparams.cpp
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

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  target_link_options(${target} PRIVATE -lexecinfo)
endif()

target_link_libraries(test-util
    ${COMMON_LINK_LIBRARIES}
)
