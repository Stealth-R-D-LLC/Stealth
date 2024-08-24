cmake_minimum_required(VERSION 3.30)

project(key-test)

include(${CMAKE_SOURCE_DIR}/../CMakeCommon.cmake)

if(DEFINED ENV{LEGACY_PKEY})
   message(STATUS "Testing for legacy EC_KEY* pkey")
   add_compile_definitions(LEGACY_PKEY=1)
endif()

add_executable(test-key)

target_sources(test-key PRIVATE
    key-test.cpp
    ${STEALTH}/wallet/key.cpp
    ${STEALTH}/client/version.cpp
    ${STEALTH}/client/sync.cpp
    ${STEALTH}/blockchain/chainparams.cpp
    ${STEALTH}/primitives/valtype.cpp
    ${STEALTH}/util/util.cpp
    ${COMMON_CPP_SOURCES}
)

target_include_directories(test-key PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STEALTH}/wallet
    ${STEALTH}/network
    ${STEALTH}/util
    ${STEALTH}/blockchain
    ${STEALTH}/client
)

if(NOT DEFINED ENV{LEGACY_PKEY})
    target_sources(test-key PRIVATE
        ${STEALTH}/crypto/core-hashes/core-hashes.cpp
    )
    target_include_directories(test-key PRIVATE
        ${STEALTH}/crypto/core-hashes
    )
endif()

target_link_libraries(test-key
    ${COMMON_LINK_LIBRARIES}
)
