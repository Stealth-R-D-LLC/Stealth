cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(Stealth VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_VERBOSE_MAKEFILE ON)


##############################################################################
## functions
##############################################################################

function(add_compile_options_with_lang_check)
  foreach(arg ${ARGN})
    if(arg MATCHES "^-std=")
      add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${arg}>)
    else()
      add_compile_options(${arg})
    endif()
  endforeach()
endfunction()


function(remove_compiler_flags)
  set(flag_vars
    CMAKE_C_FLAGS CMAKE_CXX_FLAGS
    CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO
    CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL
  )

  foreach(flag ${ARGN})
    foreach(flag_var ${flag_vars})
      string(REPLACE "${flag}" "" ${flag_var} "${${flag_var}}")
    endforeach()
  endforeach()

  foreach(flag_var ${flag_vars})
    set(${flag_var} "${${flag_var}}" PARENT_SCOPE)
  endforeach()
endfunction()


##############################################################################
## debugging and optimization
##############################################################################

remove_compiler_flags(-O0 -O1 -O2 -O3 -Os -Ofast)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-O0)
  add_link_options(-O0)
else()
  add_compile_options(-O2)
  add_link_options(-O2)
endif()


set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2")
set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -O2")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -O2")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O2")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -O2")

option(WITH_MINER "Build with miner support" OFF)
option(USE_IPV6 "Use IPv6" OFF)


##############################################################################
## build info
##############################################################################

set(BUILD_H "${CMAKE_BINARY_DIR}/build.h")
add_custom_command(
  OUTPUT ${BUILD_H}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}
  COMMAND /bin/sh ${CMAKE_SOURCE_DIR}/share/genbuild.sh ${BUILD_H}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/share/genbuild.sh
  VERBATIM
)
add_custom_target(generate_build_h DEPENDS ${BUILD_H})


##############################################################################
## compiler and linker flags
##############################################################################

add_compile_options(-Wall -Wextra -Wno-ignored-qualifiers
                    -Wformat -Wformat-security -Wno-unused-parameter)
add_definitions(-DHAVE_BUILD_INFO)

# build type
add_compile_options(-g)
add_definitions(-UDEBUG -U_DEBUG -UNDEBUG)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE
                                "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE
                                "${CMAKE_C_FLAGS_RELEASE}")

add_definitions(-DBOOST_SPIRIT_THREADSAFE)

set(CMAKE_CXX_FLAGS,
        "${CMAKE_CXX_FLAGS} -pthread -std=c++14")
set(CMAKE_CXX_FLAGS_RELEASE,
        "${CMAKE_CXX_FLAGS_RELEASE} -pthread -std=c++14")

add_compile_options_with_lang_check(-std=c++14)


# hardening
add_compile_options(-fno-stack-protector
                    -fstack-protector-all
                    -Wstack-protector
                    -D_FORTIFY_SOURCE=2)

add_link_options(-fno-stack-protector
                 -fstack-protector-all
                 -Wstack-protector)


# platform-specific flags
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_compile_options(-fPIE)
  add_link_options(-pie)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  add_link_options(-lexecinfo)
endif()


##############################################################################
## compile-time options
##############################################################################

if(WITH_MINER)
  add_definitions(-DWITH_MINER)
endif()

if(USE_IPV6)
  add_definitions(-DUSE_IPV6=${USE_IPV6})
endif()

##############################################################################
## dependencies
##############################################################################

##
## boost
##

set(BOOST_ROOT "" CACHE PATH
        "Path to Boost root directory")

if(BOOST_ROOT)
  set(BOOST_ROOT ${BOOST_ROOT} CACHE PATH
         "Path to Boost root directory" FORCE)
  set(Boost_NO_SYSTEM_PATHS ON)
endif()

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_USE_MULTITHREADED ON)

unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_LIBRARY_DIRS CACHE)
foreach(component atomic chrono system filesystem program_options thread)
  unset(Boost_${component}_FOUND CACHE)
  unset(Boost_${component}_LIBRARY CACHE)
  unset(Boost_${component}_LIBRARY_DEBUG CACHE)
  unset(Boost_${component}_LIBRARY_RELEASE CACHE)
endforeach()

find_package(Boost REQUIRED COMPONENTS
             atomic chrono system filesystem program_options thread)

if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost libraries not found. "
                      "Please specify the correct Boost root directory "
                      "using -DBOOST_ROOT=/path/to/boost")
endif()

message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
foreach(component atomic chrono system filesystem program_options thread)
  if(Boost_${component}_LIBRARY)
    message(STATUS "  Boost_${component}_LIBRARY: "
                   "${Boost_${component}_LIBRARY}")
  else()
    message(STATUS "  Boost_${component}_LIBRARY: [Not found]")
  endif()
endforeach()

function(find_dynamic_boost_library component)
  find_library(Boost_${component}_LIBRARY_DYNAMIC
    NAMES
      libboost_${component}.so
      libboost_${component}-mt.so
      libboost_${component}.dylib
      libboost_${component}-mt.dylib
      libboost_${component}.dll
      libboost_${component}-mt.dll
    HINTS
      ${Boost_LIBRARY_DIRS}
      /usr/local/lib
      /usr/lib64
      /usr/lib
    NO_DEFAULT_PATH
  )
  if(Boost_${component}_LIBRARY_DYNAMIC)
    set(Boost_${component}_LIBRARY
        ${Boost_${component}_LIBRARY_DYNAMIC}
        PARENT_SCOPE)
    message(STATUS "Found dynamic Boost_${component}_LIBRARY:
                   ${Boost_${component}_LIBRARY_DYNAMIC}")
  else()
    message(STATUS "Dynamic library for Boost ${component} not found")
  endif()
endfunction()

foreach(component atomic chrono system filesystem program_options thread)
  find_dynamic_boost_library(${component})
endforeach()

set(Boost_LIBRARIES "")
foreach(component atomic chrono system filesystem program_options thread)
  if(Boost_${component}_LIBRARY)
    list(APPEND Boost_LIBRARIES ${Boost_${component}_LIBRARY})
  endif()
endforeach()

message(STATUS "Updated Boost_LIBRARIES: ${Boost_LIBRARIES}")

##
## openssl
##

set(OPENSSL_ROOT "" CACHE PATH
        "Path to Boost root directory")

if(OPENSSL_ROOT)
  set(OPENSSL_ROOT ${OPENSSL_ROOT} CACHE PATH
          "Path to Boost root directory" FORCE)
  set(OpenSSL_NO_SYSTEM_PATHS ON)
endif()

find_package(OpenSSL REQUIRED)

##
## zlib
##

find_package(ZLIB REQUIRED)

##
## berkeley db
##

set(BDB_ROOT "" CACHE PATH "Path to Berkeley DB root directory")

function(find_bdb)
  find_path(BDB_INCLUDE_DIR db_cxx.h
    HINTS
      ${DEPS}/bdb/bdb-stealth/include
      ${BDB_ROOT}/include
      /usr/local/opt/berkeley-db/include
      /opt/homebrew/opt/berkeley-db/include
      /opt/local/include
      /sw/include
      /usr/local/include
      /usr/include
    PATH_SUFFIXES
      db18 db6 db5
    DOC "Berkeley DB include directory"
  )

  if(BDB_INCLUDE_DIR)
    get_filename_component(BDB_INCLUDE_DIR_PARENT ${BDB_INCLUDE_DIR} DIRECTORY)

    file(GLOB BDB_LIBRARY_CANDIDATES
      "${BDB_INCLUDE_DIR_PARENT}/lib/libdb_cxx-*.so"
      "${BDB_INCLUDE_DIR_PARENT}/lib/libdb_cxx-*.dylib"
      "${BDB_INCLUDE_DIR_PARENT}/lib/libdb_cxx-*.a"
      "${BDB_INCLUDE_DIR_PARENT}/lib/libdb_cxx-*.dll"
      "/usr/local/lib/libdb_cxx-*.so"
      "/usr/local/lib/libdb_cxx-*.dylib"
      "/usr/local/lib/libdb_cxx-*.a"
      "/usr/local/lib/libdb_cxx-*.dll"
      "/usr/lib/libdb_cxx-*.so"
      "/usr/lib/libdb_cxx-*.dylib"
      "/usr/lib/libdb_cxx-*.a"
      "/usr/lib/libdb_cxx-*.dll"
    )

    if(BDB_LIBRARY_CANDIDATES)
      list(GET BDB_LIBRARY_CANDIDATES 0 BDB_LIBRARY)
    else()
      find_library(BDB_LIBRARY
        NAMES db_cxx
        HINTS
          ${BDB_INCLUDE_DIR_PARENT}/lib
          /usr/local/lib
          /usr/lib
        DOC "Berkeley DB library"
      )
    endif()
  endif()

  if(BDB_INCLUDE_DIR AND BDB_LIBRARY)
    set(BDB_FOUND TRUE PARENT_SCOPE)
    set(BDB_INCLUDE_DIR ${BDB_INCLUDE_DIR} PARENT_SCOPE)
    set(BDB_LIBRARY ${BDB_LIBRARY} PARENT_SCOPE)
  else()
    set(BDB_FOUND FALSE PARENT_SCOPE)
  endif()
endfunction()

find_bdb()

if(NOT BDB_FOUND)
  message(FATAL_ERROR "Berkeley DB not found. "
                      "Please install Berkeley DB or specify "
                      "its location with -DBDB_ROOT=<path>")
endif()

message(STATUS "Found Berkeley DB:")
message(STATUS "  Include dir: ${BDB_INCLUDE_DIR}")
message(STATUS "  Library: ${BDB_LIBRARY}")

##
## libevent
##

find_path(EVENT_INCLUDE_DIR event2/event.h
  HINTS ${EVENT_ROOT}/include /usr/local/include /usr/include
  DOC "libevent include directory")
find_library(EVENT_LIBRARY
  NAMES event
  HINTS ${EVENT_ROOT}/lib /usr/local/lib /usr/lib
  DOC "libevent library")

if(NOT EVENT_INCLUDE_DIR OR NOT EVENT_LIBRARY)
  message(FATAL_ERROR "libevent not found. "
                      "Please install libevent or specify "
                      "its location with -DEVENT_ROOT=<path>")
endif()

##
## crypto++
##

find_path(CRYPTOPP_INCLUDE_DIR cryptopp/config.h
  HINTS ${CRYPTOPP_ROOT}/include /usr/local/include /usr/include
  DOC "Crypto++ include directory")
find_library(CRYPTOPP_LIBRARY
  NAMES cryptopp
  HINTS ${CRYPTOPP_ROOT}/lib /usr/local/lib /usr/lib
  DOC "Crypto++ library")

if(NOT CRYPTOPP_INCLUDE_DIR OR NOT CRYPTOPP_LIBRARY)
  message(FATAL_ERROR "Crypto++ not found. "
                      "Please install Crypto++ or specify "
                      "its location with -DCRYPTOPP_ROOT=<path>")
endif()

##############################################################################
## subdirectories
##############################################################################

#
# leveldb
#

set(LEVELDB_BUILD_TESTS OFF CACHE BOOL
                    "Build LevelDB's unit tests" FORCE)
set(LEVELDB_BUILD_BENCHMARKS OFF CACHE BOOL
                    "Build LevelDB's benchmarks" FORCE)
add_subdirectory(src/leveldb)

message(STATUS "Boost includes: ${Boost_INCLUDE_DIRS}")
message(STATUS Boost::thread)


##############################################################################
## sources
##############################################################################

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/json
  ${CMAKE_CURRENT_SOURCE_DIR}/src/tor
  ${CMAKE_CURRENT_SOURCE_DIR}/src/tor/adapter
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bip32
  ${CMAKE_CURRENT_SOURCE_DIR}/src/blockchain
  ${CMAKE_CURRENT_SOURCE_DIR}/src/client
  ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/argon2/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/core-hashes
  ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/hashblock
  ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/xorshift1024
  ${CMAKE_CURRENT_SOURCE_DIR}/src/primitives
  ${CMAKE_CURRENT_SOURCE_DIR}/src/db-bdb
  ${CMAKE_CURRENT_SOURCE_DIR}/src/db-leveldb
  ${CMAKE_CURRENT_SOURCE_DIR}/src/explore
  ${CMAKE_CURRENT_SOURCE_DIR}/src/feeless
  ${CMAKE_CURRENT_SOURCE_DIR}/src/network
  ${CMAKE_CURRENT_SOURCE_DIR}/src/qpos
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util
  ${CMAKE_CURRENT_SOURCE_DIR}/src/wallet
  ${Boost_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIRS}
  ${LEVELDB_INCLUDE_DIR}
  ${BDB_INCLUDE_DIR}
  ${EVENT_INCLUDE_DIR}
  ${CRYPTOPP_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
)

##
## globs and filters
##

# core
file(GLOB_RECURSE SOURCES
  "src/*.cpp"
  "src/*.c"
)

list(FILTER SOURCES EXCLUDE REGEX ".*/leveldb/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*qt/.*")
list(FILTER SOURCES EXCLUDE REGEX "src/tor/.*")
list(FILTER SOURCES EXCLUDE REGEX "src/test/.*")

list(FILTER SOURCES EXCLUDE REGEX "src/crypto/argon2/src/opt\\.c$")

# tor
file(GLOB_RECURSE TOR_SOURCES
  "src/tor/*.c"
  "src/tor/*.cpp"
)

list(FILTER TOR_SOURCES EXCLUDE REGEX "src/tor/.*_nss.*\\.c$")
list(FILTER TOR_SOURCES EXCLUDE REGEX "src/tor/.*nss_.*\\.c$")
list(FILTER TOR_SOURCES EXCLUDE REGEX "src/tor/.*tinytest.*")
list(FILTER TOR_SOURCES EXCLUDE REGEX
         "src/tor/ext/curve25519_donna/curve25519-donna-c64\\.c")
list(FILTER TOR_SOURCES EXCLUDE REGEX "src/tor/ext/timeouts/test-timeout\\.c")
list(FILTER TOR_SOURCES EXCLUDE REGEX "src/tor/.*_stub\\.c")
list(FILTER TOR_SOURCES EXCLUDE REGEX "src/tor/.*OpenBSD_malloc_Linux\\.c")


if(NOT WIN32)
  list(FILTER TOR_SOURCES EXCLUDE REGEX
           "src/tor/lib/compat_mutex_winthreads\\.c$")
  list(FILTER TOR_SOURCES EXCLUDE REGEX
           "src/tor/lib/compat_winthreads\\.c$")
endif()


##############################################################################
## StealthCoind
##############################################################################

add_executable(StealthCoind)

target_sources(StealthCoind PRIVATE
  ${SOURCES}
  ${TOR_SOURCES}
)

target_link_libraries(StealthCoind
  PRIVATE
  Boost::atomic
  Boost::chrono
  Boost::filesystem
  Boost::program_options
  Boost::system
  Boost::thread
  OpenSSL::SSL
  OpenSSL::Crypto
  ZLIB::ZLIB
  ${BDB_LIBRARY}
  ${EVENT_LIBRARY}
  ${CRYPTOPP_LIBRARY}
  ${CMAKE_DL_LIBS}
  leveldb
  dl
  pthread
)

add_dependencies(StealthCoind generate_build_h)

install(TARGETS StealthCoind DESTINATION bin)
