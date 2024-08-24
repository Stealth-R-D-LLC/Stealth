cmake_minimum_required(VERSION 3.30)

project(util-test)

include(${CMAKE_SOURCE_DIR}/../CMakeCommon.cmake)

add_executable(test-util)

target_sources(test-util PRIVATE
    util-test.cpp
    ${STEALTH}/util/util.cpp
    ${STEALTH}/client/version.cpp
    ${STEALTH}/blockchain/chainparams.cpp
    ${COMMON_CPP_SOURCES}
)

if (NOT DEFINED EXTERNAL_STEALTH)
    target_sources(test-util PRIVATE
        ${STEALTH}/crypto/core-hashes/core-hashes.cpp
    )
endif()

target_include_directories(test-util PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STEALTH}/util
    ${STEALTH}/client
    ${STEALTH}/crypto/core-hashes
    ${STEALTH}/blockchain
)

if (NOT DEFINED EXTERNAL_STEALTH)
    target_include_directories(test-util PRIVATE
        ${STEALTH}/crypto/core-hashes
    )
endif()

target_link_libraries(test-util
    ${COMMON_LINK_LIBRARIES}
)

