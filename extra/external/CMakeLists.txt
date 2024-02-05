# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(DSE_MODELC__External_Projects)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS_DEBUG "-g -ggdb")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O3")
add_compile_options(-Wall -W -Wwrite-strings -Wno-missing-field-initializers)



# Load URLs
# =========
include(oss_repos.cmake)
if($ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()



# External Projects
# =================
include(ExternalProject)
set_property(DIRECTORY PROPERTY EP_STEP_TARGETS download)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")


# DSE C Lib
# ---------
include(FetchContent)
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{DSE_CLIB_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_CLIB_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.clib"
)
FetchContent_MakeAvailable(dse_clib)

# DSE Network Codec
# -----------------
ExternalProject_Add(dse_ncodec
    URL                 ${ExternalProject__DSE_NCODEC__URL}
    HTTP_USERNAME       ${ExternalProject__DSE_NCODEC__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__DSE_NCODEC__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse_ncodec"
    SOURCE_SUBDIR       "dse/ncodec/libs/automotive-bus"
    BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse_ncodec"
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    INSTALL_COMMAND     ""
)


# Lib Event
# ---------
ExternalProject_Add(event
    URL                 ${ExternalProject__EVENT__URL}
    HTTP_USERNAME       ${ExternalProject__EVENT__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__EVENT__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/event"
    BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/event"
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DEVENT__DISABLE_OPENSSL=ON
        -DEVENT__DISABLE_MBEDTLS=ON
        -DEVENT__DISABLE_DEBUG_MODE=ON
        -DEVENT__DISABLE_TESTS=ON
        -DEVENT__DISABLE_REGRESS=ON
        -DEVENT__DISABLE_SAMPLES=ON
    INSTALL_COMMAND     ""
)

# Lib YAML
# --------
ExternalProject_Add(yaml
    URL                 ${ExternalProject__YAML__URL}
    HTTP_USERNAME       ${ExternalProject__YAML__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__YAML__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/yaml"
    BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/yaml"
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DBUILD_SHARED_LIBS=OFF
    INSTALL_COMMAND     ""
)

# MsgpackC
# --------
ExternalProject_Add(msgpackc
    URL                 ${ExternalProject__MSGPACK__URL}
    HTTP_USERNAME       ${ExternalProject__MSGPACK__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__MSGPACK__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/msgpackc"
    BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/msgpackc-build"
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DBUILD_SHARED_LIBS=OFF
        -DMSGPACK_BUILD_EXAMPLES=OFF
        -DMSGPACK_BUILD_TESTS=OFF
        -DMSGPACK_GEN_COVERAGE=OFF
    INSTALL_COMMAND     ""
)

# HiRedis
# -------
# HiRedis depends on LibEvent and needs its include path set correctly to
# reference the above Lib Event external project (/tmp/dse.modelc/event/include).
#
# NOTE: Working with a Mirrored and Patched version of HiRedis.
ExternalProject_Add(hiredis
    URL                 ${ExternalProject__HIREDIS__URL}
    HTTP_USERNAME       ${ExternalProject__HIREDIS__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__HIREDIS__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/hiredis"
    BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/hiredis-build"
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DENABLE_SSL=OFF
        -DDISABLE_TESTS=ON
        -DENABLE_SSL_TESTS=OFF
    BUILD_COMMAND       ${CMAKE_MAKE_PROGRAM} -e CFLAGS=-I$ENV{EXTERNAL_BUILD_DIR}/event/include
    INSTALL_COMMAND     ""
)

# dlfcn-win32
# -----------
if(NOT WIN32)
    set(DLFCNWIN32_BUILD_COMMAND "")
endif()
ExternalProject_Add(dlfcnwin32
    URL                 ${ExternalProject__DLFCNWIN32__URL}
    HTTP_USERNAME       ${ExternalProject__DLFCNWIN32__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__DLFCNWIN32__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32"
    BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32-build"
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTS=OFF
    BUILD_COMMAND       ${DLFCNWIN32_BUILD_COMMAND}
    INSTALL_COMMAND     ""
)



# OSS Target
# ==========
add_custom_target(oss)
function(oss_copy src dst)
    add_custom_command(TARGET oss PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${src}"
            "../../../dse/__oss__/${dst}"
    )
endfunction()
add_dependencies(oss
    dlfcnwin32-download
    dse_ncodec-download
    event-download
    hiredis-download
    msgpackc-download
    yaml-download
)
oss_copy(${dse_clib_SOURCE_DIR} dse.clib)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32 dlfcnwin32)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/dse_ncodec dse_ncodec)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/event event)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/hiredis hiredis)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/msgpackc msgpackc)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/yaml yaml)
