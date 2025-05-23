cmake_minimum_required(VERSION 3.12)
project(pyHash9 VERSION 1.0 LANGUAGES C CXX)

set(CUSTOM_PYTHON_PATH "" CACHE PATH "Path to custom Python installation")

if(CUSTOM_PYTHON_PATH)
    set(Python_ROOT_DIR ${CUSTOM_PYTHON_PATH})
    message(STATUS "Using custom Python path: ${CUSTOM_PYTHON_PATH}")
elseif(DEFINED ENV{CUSTOM_PYTHON_PATH})
    set(Python_ROOT_DIR $ENV{CUSTOM_PYTHON_PATH})
    message(STATUS "Using custom Python path from environment: "
                      "$ENV{CUSTOM_PYTHON_PATH}")
endif()

find_package(Python 3 REQUIRED COMPONENTS Interpreter Development.Module)
message(STATUS "Found Python: ${Python_EXECUTABLE}")
message(STATUS "Python Include Dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Python Libraries: ${Python_LIBRARIES}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC")

set(STEALTH_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

include_directories(${Python_INCLUDE_DIRS})
include_directories(${STEALTH_ROOT}/src/crypto/hashblock)
include_directories(${STEALTH_ROOT}/src/primitives)

set(HASHBLOCK_SOURCES
    ${STEALTH_ROOT}/src/crypto/hashblock/blake.c
    ${STEALTH_ROOT}/src/crypto/hashblock/bmw.c
    ${STEALTH_ROOT}/src/crypto/hashblock/groestl.c
    ${STEALTH_ROOT}/src/crypto/hashblock/jh.c
    ${STEALTH_ROOT}/src/crypto/hashblock/keccak.c
    ${STEALTH_ROOT}/src/crypto/hashblock/skein.c
    ${STEALTH_ROOT}/src/crypto/hashblock/luffa.c
    ${STEALTH_ROOT}/src/crypto/hashblock/cubehash.c
    ${STEALTH_ROOT}/src/crypto/hashblock/shavite.c
    ${STEALTH_ROOT}/src/crypto/hashblock/simd.c
    ${STEALTH_ROOT}/src/crypto/hashblock/echo.c
    ${STEALTH_ROOT}/src/crypto/hashblock/hamsi.c
    ${STEALTH_ROOT}/src/crypto/hashblock/fugue.c
)

set(PYHASH9_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/pyHash9.cpp
)

add_library(pyHash9 MODULE ${HASHBLOCK_SOURCES} ${PYHASH9_SOURCES})

set_target_properties(pyHash9 PROPERTIES 
    PREFIX "" 
    OUTPUT_NAME "pyHash9"
    POSITION_INDEPENDENT_CODE ON
    SUFFIX ".so"
)

target_link_libraries(pyHash9 PRIVATE Python::Module)

include(GNUInstallDirs)
if(DEFINED ENV{VIRTUAL_ENV} OR DEFINED Python_SITELIB)
    set(PYTHON_SITE_PACKAGES ${Python_SITELIB})
else()
    execute_process(
        COMMAND ${Python_EXECUTABLE} -c "import site; print(site.USER_SITE)"
        OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

install(TARGETS pyHash9
        LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES}
        COMPONENT python)

file(COPY ${CMAKE_SOURCE_DIR}/test-pyHash9.py
     DESTINATION ${CMAKE_BINARY_DIR})

message(STATUS "pyHash9 will be installed to: ${PYTHON_SITE_PACKAGES}")
