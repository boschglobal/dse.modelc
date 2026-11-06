# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)
if(TARGET dse_external_hiredis)
    return()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/event.cmake)
include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/dse_external.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/oss_repos.cmake)
if(DEFINED ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()

set(DSE_EXTERNAL_HIREDIS_PREFIX "${CMAKE_BINARY_DIR}/_deps/dse_external_hiredis")
set(DSE_EXTERNAL_HIREDIS_INSTALL_DIR "${DSE_EXTERNAL_HIREDIS_PREFIX}/install")
set(DSE_EXTERNAL_HIREDIS_LIB "${DSE_EXTERNAL_HIREDIS_INSTALL_DIR}/lib/libhiredis.a")
set(DSE_EXTERNAL_HIREDIS_INCLUDE_DIR "${DSE_EXTERNAL_HIREDIS_INSTALL_DIR}/include")
get_target_property(DSE_EXTERNAL_HIREDIS_EVENT_INCLUDE_DIR dse_external_event INTERFACE_INCLUDE_DIRECTORIES)

# NOTE: REPO_MIRRORS may point to a mirrored and patched hiredis which needs libevent headers.
dse_external_download(hiredis "${ExternalProject__HIREDIS__URL}"
    "${ExternalProject__HIREDIS__USERNAME}" "${ExternalProject__HIREDIS__PASSWORD}" DSE_EXTERNAL_HIREDIS_ARCHIVE)
ExternalProject_Add(dse_external_hiredis_build
    URL                 "${DSE_EXTERNAL_HIREDIS_ARCHIVE}"
    URL_HASH            ${DSE_EXTERNAL_HIREDIS_ARCHIVE_HASH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX              "${DSE_EXTERNAL_HIREDIS_PREFIX}"
    INSTALL_DIR         "${DSE_EXTERNAL_HIREDIS_INSTALL_DIR}"
    EXCLUDE_FROM_ALL    TRUE
    DEPENDS             dse_external_event_build
    CMAKE_ARGS
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        -Wno-dev
        -Wno-deprecated
        "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS} -I${DSE_EXTERNAL_HIREDIS_EVENT_INCLUDE_DIR}"
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_INSTALL_LIBDIR=lib
        -DBUILD_SHARED_LIBS=OFF
        -DENABLE_SSL=OFF
        -DDISABLE_TESTS=ON
        -DENABLE_SSL_TESTS=OFF
    BUILD_BYPRODUCTS    "${DSE_EXTERNAL_HIREDIS_LIB}"
)

# Imported targets require their include directories to exist at generate time.
file(MAKE_DIRECTORY "${DSE_EXTERNAL_HIREDIS_INCLUDE_DIR}/hiredis")
add_library(dse_external_hiredis STATIC IMPORTED GLOBAL)
set_target_properties(dse_external_hiredis
    PROPERTIES
        IMPORTED_LOCATION "${DSE_EXTERNAL_HIREDIS_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DSE_EXTERNAL_HIREDIS_INCLUDE_DIR};${DSE_EXTERNAL_HIREDIS_INCLUDE_DIR}/hiredis"
        INTERFACE_LINK_LIBRARIES dse_external_event
)
add_dependencies(dse_external_hiredis dse_external_hiredis_build)
