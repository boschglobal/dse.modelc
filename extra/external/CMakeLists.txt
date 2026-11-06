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
if(DEFINED ENV{REPO_MIRRORS})
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
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.ncodec"
)
FetchContent_MakeAvailable(dse_ncodec)


# OSS Sources
# ===========
include(event.cmake)
include(hiredis.cmake)
include(yaml.cmake)
include(dlfcnwin32.cmake)


# OSS Target
# ==========
add_custom_target(oss)

function(oss_copy src dst)
    if(TARGET ${src})
        add_dependencies(oss ${src}-download)
        ExternalProject_Get_Property(${src} SOURCE_DIR)
        set(src ${SOURCE_DIR})
    endif()
    add_custom_command(TARGET oss PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${src}"
            "../../../dse/__oss__/${dst}"
    )
endfunction()
oss_copy(${dse_clib_SOURCE_DIR}                 boschglobal/dse.clib)
oss_copy(${dse_schemas_SOURCE_DIR}              boschglobal/dse.schemas)
oss_copy(${dse_ncodec_SOURCE_DIR}               boschglobal/dse.ncodec)
oss_copy(dse_external_dl_build                  dlfcn-win32/dlfcn-win32)
oss_copy(dse_external_event_build               libevent/libevent)
oss_copy(dse_external_hiredis_build             redis/hiredis)
oss_copy(dse_external_yaml_build                yaml/libyaml)
