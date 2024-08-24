cmake_minimum_required(VERSION 3.30)

project(core-hashes-test)

include(${CMAKE_SOURCE_DIR}/../CMakeCommon.cmake)

add_executable(test-core-hashes)

target_sources(test-core-hashes PRIVATE
    core-hashes-test.cpp
    ${STEALTH}/crypto/core-hashes/core-hashes.cpp
    ${COMMON_CPP_SOURCES}
)

target_include_directories(test-core-hashes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STEALTH}/crypto/core-hashes
)

target_link_libraries(test-core-hashes
    ${COMMON_LINK_LIBRARIES}
)
