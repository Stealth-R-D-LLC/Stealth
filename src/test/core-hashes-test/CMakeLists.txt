cmake_minimum_required(VERSION 3.0)

project(core-hashes-test)

set(target test-core-hashes)
add_executable(${target})

include(${CMAKE_SOURCE_DIR}/../CMakeCommon.cmake)

set(C_SOURCES
    ${STEALTH}/crypto/core-hashes/memzero.c
    ${STEALTH}/crypto/core-hashes/ripemd160.c
    ${STEALTH}/crypto/core-hashes/sha2.c
)

target_sources(${target} PRIVATE
    core-hashes-test.cpp
    ${STEALTH}/crypto/core-hashes/core-hashes.cpp
    ${C_SOURCES}
    ${COMMON_CPP_SOURCES}
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-std=c11>
)
endif()

set_source_files_properties(${C_SOURCES} PROPERTIES
    LANGUAGE C
)

target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STEALTH}/crypto/core-hashes
)

target_link_libraries(${target}
    ${COMMON_LINK_LIBRARIES}
)
