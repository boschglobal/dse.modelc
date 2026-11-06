# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)
if(TARGET dse_external_event)
    return()
endif()

include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/dse_external.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/oss_repos.cmake)
if(DEFINED ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()

set(DSE_EXTERNAL_EVENT_PREFIX "${CMAKE_BINARY_DIR}/_deps/dse_external_event")
set(DSE_EXTERNAL_EVENT_INSTALL_DIR "${DSE_EXTERNAL_EVENT_PREFIX}/install")
set(DSE_EXTERNAL_EVENT_LIB "${DSE_EXTERNAL_EVENT_INSTALL_DIR}/lib/libevent.a")
set(DSE_EXTERNAL_EVENT_INCLUDE_DIR "${DSE_EXTERNAL_EVENT_INSTALL_DIR}/include")

# Upstream warnings on Windows (time_t, GetProcAddress casts, "%I64d") are harmless.
set(DSE_EXTERNAL_EVENT_C_FLAGS "${CMAKE_C_FLAGS}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(DSE_EXTERNAL_EVENT_C_FLAGS "${DSE_EXTERNAL_EVENT_C_FLAGS} -Wno-incompatible-pointer-types -Wno-cast-function-type -Wno-format")
endif()

dse_external_download(event "${ExternalProject__EVENT__URL}"
    "${ExternalProject__EVENT__USERNAME}" "${ExternalProject__EVENT__PASSWORD}" DSE_EXTERNAL_EVENT_ARCHIVE)
ExternalProject_Add(dse_external_event_build
    URL                 "${DSE_EXTERNAL_EVENT_ARCHIVE}"
    URL_HASH            ${DSE_EXTERNAL_EVENT_ARCHIVE_HASH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX              "${DSE_EXTERNAL_EVENT_PREFIX}"
    INSTALL_DIR         "${DSE_EXTERNAL_EVENT_INSTALL_DIR}"
    EXCLUDE_FROM_ALL    TRUE
    CMAKE_ARGS
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        -Wno-dev
        -Wno-deprecated
        "-DCMAKE_C_FLAGS=${DSE_EXTERNAL_EVENT_C_FLAGS}"
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON
        -DEVENT__LIBRARY_TYPE=STATIC
        -DEVENT__DISABLE_OPENSSL=ON
        -DEVENT__DISABLE_DEBUG_MODE=ON
        -DEVENT__DISABLE_BENCHMARK=ON
        -DEVENT__DISABLE_TESTS=ON
        -DEVENT__DISABLE_REGRESS=ON
        -DEVENT__DISABLE_SAMPLES=ON
    BUILD_BYPRODUCTS    "${DSE_EXTERNAL_EVENT_LIB}"
)

# Imported targets require their include directories to exist at generate time.
file(MAKE_DIRECTORY "${DSE_EXTERNAL_EVENT_INCLUDE_DIR}")
add_library(dse_external_event STATIC IMPORTED GLOBAL)
set_target_properties(dse_external_event
    PROPERTIES
        IMPORTED_LOCATION "${DSE_EXTERNAL_EVENT_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DSE_EXTERNAL_EVENT_INCLUDE_DIR}"
)
add_dependencies(dse_external_event dse_external_event_build)
