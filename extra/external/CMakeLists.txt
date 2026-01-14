# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

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

# External Project - DSE Schemas
# ------------------------------
FetchContent_Declare(dse_schemas
    URL                 $ENV{DSE_SCHEMA_URL}
    HTTP_USERNAME       $ENV{DSE_SCHEMA_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_SCHEMA_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.schemas"
)
FetchContent_MakeAvailable(dse_schemas)

# DSE Network Codec
# -----------------
include(FetchContent)
FetchContent_Declare(dse_ncodec
    URL                 $ENV{DSE_NCODEC_URL}
    HTTP_USERNAME       $ENV{DSE_NCODEC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_NCODEC_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.ncodec/dse/ncodec"
)
FetchContent_MakeAvailable(dse_ncodec)


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

# Lua
# ---
if(UNIX)
ExternalProject_Add(lua
    URL                 ${ExternalProject__LUA__URL}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/lua"
    BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/lua"
    PATCH_COMMAND       ${CMAKE_COMMAND} -E copy
                        ${CMAKE_CURRENT_SOURCE_DIR}/lua_makefile
                        <SOURCE_DIR>/makefile
    CONFIGURE_COMMAND   ""
    BUILD_COMMAND       make
                        CMAKE_C_FLAGS=${CMAKE_C_FLAGS}
                        CC=${CMAKE_C_COMPILER}
    INSTALL_COMMAND     ""
)
endif()


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
    event-download
    hiredis-download
    yaml-download
)
oss_copy(${dse_clib_SOURCE_DIR}                 boschglobal/dse.clib)
oss_copy(${dse_schemas_SOURCE_DIR}              boschglobal/dse.schemas)
oss_copy(${dse_ncodec_SOURCE_DIR}               boschglobal/dse.ncodec)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32    dlfcn-win32/dlfcn-win32)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/event         libevent/libevent)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/hiredis       redis/hiredis)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/yaml          yaml/libyaml)
