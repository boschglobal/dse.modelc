# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)
if(TARGET dse_external_yaml)
    return()
endif()

include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/dse_external.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/oss_repos.cmake)
if(DEFINED ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()

set(DSE_EXTERNAL_YAML_PREFIX "${CMAKE_BINARY_DIR}/_deps/dse_external_yaml")
set(DSE_EXTERNAL_YAML_INSTALL_DIR "${DSE_EXTERNAL_YAML_PREFIX}/install")
set(DSE_EXTERNAL_YAML_LIB "${DSE_EXTERNAL_YAML_INSTALL_DIR}/lib/libyaml.a")
set(DSE_EXTERNAL_YAML_INCLUDE_DIR "${DSE_EXTERNAL_YAML_INSTALL_DIR}/include")

dse_external_download(yaml "${ExternalProject__YAML__URL}"
    "${ExternalProject__YAML__USERNAME}" "${ExternalProject__YAML__PASSWORD}" DSE_EXTERNAL_YAML_ARCHIVE)
ExternalProject_Add(dse_external_yaml_build
    URL                 "${DSE_EXTERNAL_YAML_ARCHIVE}"
    URL_HASH            ${DSE_EXTERNAL_YAML_ARCHIVE_HASH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX              "${DSE_EXTERNAL_YAML_PREFIX}"
    INSTALL_DIR         "${DSE_EXTERNAL_YAML_INSTALL_DIR}"
    EXCLUDE_FROM_ALL    TRUE
    CMAKE_ARGS
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        -Wno-dev
        -Wno-deprecated
        "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTING=OFF
    BUILD_BYPRODUCTS    "${DSE_EXTERNAL_YAML_LIB}"
)

# Imported targets require their include directories to exist at generate time.
file(MAKE_DIRECTORY "${DSE_EXTERNAL_YAML_INCLUDE_DIR}")
add_library(dse_external_yaml STATIC IMPORTED GLOBAL)
set_target_properties(dse_external_yaml
    PROPERTIES
        IMPORTED_LOCATION "${DSE_EXTERNAL_YAML_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DSE_EXTERNAL_YAML_INCLUDE_DIR}"
)
add_dependencies(dse_external_yaml dse_external_yaml_build)
