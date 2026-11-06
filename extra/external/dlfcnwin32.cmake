# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)
if(TARGET dse_external_dl)
    return()
endif()

include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/dse_external.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/oss_repos.cmake)
if(DEFINED ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()

set(DSE_EXTERNAL_DL_PREFIX "${CMAKE_BINARY_DIR}/_deps/dse_external_dlfcnwin32")
set(DSE_EXTERNAL_DL_INSTALL_DIR "${DSE_EXTERNAL_DL_PREFIX}/install")
set(DSE_EXTERNAL_DL_LIB "${DSE_EXTERNAL_DL_INSTALL_DIR}/lib/libdl.a")
set(DSE_EXTERNAL_DL_INCLUDE_DIR "${DSE_EXTERNAL_DL_INSTALL_DIR}/include")
dse_external_download(dlfcnwin32 "${ExternalProject__DLFCNWIN32__URL}"
    "${ExternalProject__DLFCNWIN32__USERNAME}" "${ExternalProject__DLFCNWIN32__PASSWORD}" DSE_EXTERNAL_DL_ARCHIVE)

if(NOT WIN32)
    # Source only (e.g. for OSS archives), the platform dl library is linked.
    ExternalProject_Add(dse_external_dl_build
        URL                 "${DSE_EXTERNAL_DL_ARCHIVE}"
        URL_HASH            ${DSE_EXTERNAL_DL_ARCHIVE_HASH}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        PREFIX              "${DSE_EXTERNAL_DL_PREFIX}"
        EXCLUDE_FROM_ALL    TRUE
        CONFIGURE_COMMAND   ""
        BUILD_COMMAND       ""
        INSTALL_COMMAND     ""
    )
    add_library(dse_external_dl INTERFACE IMPORTED GLOBAL)
    set_target_properties(dse_external_dl
        PROPERTIES
            INTERFACE_LINK_LIBRARIES "${CMAKE_DL_LIBS}"
    )
    return()
endif()

ExternalProject_Add(dse_external_dl_build
    URL                 "${DSE_EXTERNAL_DL_ARCHIVE}"
    URL_HASH            ${DSE_EXTERNAL_DL_ARCHIVE_HASH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX              "${DSE_EXTERNAL_DL_PREFIX}"
    INSTALL_DIR         "${DSE_EXTERNAL_DL_INSTALL_DIR}"
    EXCLUDE_FROM_ALL    TRUE
    CMAKE_ARGS
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        -Wno-dev
        -Wno-deprecated
        "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTS=OFF
    BUILD_BYPRODUCTS    "${DSE_EXTERNAL_DL_LIB}"
)

# Imported targets require their include directories to exist at generate time.
file(MAKE_DIRECTORY "${DSE_EXTERNAL_DL_INCLUDE_DIR}")
add_library(dse_external_dl STATIC IMPORTED GLOBAL)
set_target_properties(dse_external_dl
    PROPERTIES
        IMPORTED_LOCATION "${DSE_EXTERNAL_DL_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DSE_EXTERNAL_DL_INCLUDE_DIR}"
)
add_dependencies(dse_external_dl dse_external_dl_build)
