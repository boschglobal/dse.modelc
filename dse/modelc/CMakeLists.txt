# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)
set(VERSION "$ENV{PACKAGE_VERSION}")

project(ModelC
    VERSION ${VERSION}
    DESCRIPTION "DSE Model C including: SimBus and ModelC."
    HOMEPAGE_URL "${PROJECT_URL}"
)

include(GNUInstallDirs)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/_out)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb")

list(APPEND C_CXX_WARNING_FLAGS
    -Wall
    -W
    -Wwrite-strings
    -Wno-missing-field-initializers
    -Wno-misleading-indentation
)
add_compile_options(${C_CXX_WARNING_FLAGS})
add_compile_definitions(DLL_BUILD)


# External Project - DSE C Lib
# ----------------------------
include(FetchContent)
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{DSE_CLIB_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_CLIB_URL_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse)
set(DSE_CLIB_SOURCE_FILES
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/hashmap.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/queue.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/set.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/binary.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/strings.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/yaml.c
)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/..")



# External Projects (targets common to sub projects)
# =================

# External Project - event
# ------------------------
set(EVENT_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/event")
set(EVENT_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/event/lib")
find_library(EVENT_LIB
    NAMES
        libevent.a
    PATHS
        ${EVENT_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(event STATIC IMPORTED GLOBAL)
set_target_properties(event
    PROPERTIES
        IMPORTED_LOCATION "${EVENT_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${EVENT_SOURCE_DIR}"
)


# External Project - msgpackc
# ---------------------------
set(MSGPACKC_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/msgpackc")
set(MSGPACKC_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/msgpackc-build")
find_library(MSGPACKC_LIB
    NAMES
        libmsgpackc.a
    PATHS
        ${MSGPACKC_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(msgpackc STATIC IMPORTED GLOBAL)
set_target_properties(msgpackc
    PROPERTIES
        IMPORTED_LOCATION "${MSGPACKC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${MSGPACKC_BINARY_DIR}"
)


# External Project - hiredis
# --------------------------
set(HIREDIS_INCLUDE_DIR "$ENV{EXTERNAL_BUILD_DIR}")
set(HIREDIS_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/hiredis")
set(HIREDIS_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/hiredis-build")
find_library(HIREDIS_LIB
    NAMES
        libhiredis.a
    PATHS
        ${HIREDIS_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(hiredis STATIC IMPORTED GLOBAL)
set_target_properties(hiredis
    PROPERTIES
        IMPORTED_LOCATION "${HIREDIS_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${HIREDIS_SOURCE_DIR}"
)


# External Project - yaml
# -----------------------
set(YAML_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/yaml")
set(YAML_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/yaml")
find_library(YAML_LIB
    NAMES
        libyaml.a
    PATHS
        ${YAML_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(yaml STATIC IMPORTED GLOBAL)
set_target_properties(yaml
    PROPERTIES
        IMPORTED_LOCATION "${YAML_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${YAML_BINARY_DIR}"
)


# External Project - dlfcn-win32
# ------------------------------
if(WIN32)
set(DLFCNWIN32_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32/src")
set(DLFCNWIN32_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32-build/lib")
find_library(DLFCNWIN32_LIB
    NAMES
        libdl.a
    PATHS
        ${DLFCNWIN32_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(dl STATIC IMPORTED GLOBAL)
set_target_properties(dl
    PROPERTIES
        IMPORTED_LOCATION "${DLFCNWIN32_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DLFCNWIN32_SOURCE_DIR}"
)
endif()


# Targets
# =======
set(DSE_MODELC_PUBLIC_HEADERS
    mcl.h
    model.h
    schema.h
)
add_subdirectory(adapter)
add_subdirectory(controller)
add_subdirectory(model)

# Model C (static)
# -------
if(UNIX)
add_library(modelc_static_lib STATIC
    ${DSE_CLIB_SOURCE_FILES}
)
set_target_properties(modelc_static_lib
    PROPERTIES
        OUTPUT_NAME modelc
)
target_include_directories(modelc_static_lib
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
)
target_link_libraries(modelc_static_lib
    PUBLIC
        adapter
        controller
        model_api
    PRIVATE
        mcheck
)
# Create bundle with all external dependencies.
include(../../extra/cmake/bundle_static_library.cmake)
bundle_static_library(modelc_static_lib modelc_bundled)
get_target_property(modelc_bundle_file modelc_bundled IMPORTED_LOCATION)
set(modelc_link_lib modelc_static_lib)
endif()

# Model C (shared)
# -------
add_library(modelc_shared_lib SHARED
    ${DSE_CLIB_SOURCE_FILES}
)
set_target_properties(modelc_shared_lib
    PROPERTIES
        OUTPUT_NAME modelc
)
target_include_directories(modelc_shared_lib
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
)
target_link_libraries(modelc_shared_lib
    PUBLIC
        adapter
        controller
        model_api
    PRIVATE
        $<$<BOOL:${WIN32}>:ws2_32>
        $<$<BOOL:${WIN32}>:iphlpapi>
        $<$<AND:$<BOOL:${WIN32}>,$<STREQUAL:${CMAKE_CXX_COMPILER_ID},"GNU">>:"-static winpthread">
)
if(NOT DEFINED modelc_link_lib)
    set(modelc_link_lib modelc_shared_lib)
endif()

# Model C install
# ---------------
if(UNIX)
install(
    TARGETS
        modelc_static_lib
    EXPORT
        modelc_bundled
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}
    COMPONENT
        modelc
)
install(
    FILES
        ${modelc_bundle_file}
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}
    COMPONENT
        modelc
)
endif()
install(
    TARGETS
        modelc_shared_lib
    LIBRARY DESTINATION
        ${CMAKE_INSTALL_LIBDIR}
    COMPONENT
        modelc
)
install(
    DIRECTORY
        ${DSE_CLIB_SOURCE_DIR}/
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/dse
    COMPONENT
        modelc
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*fmi*" EXCLUDE
        PATTERN "*process*" EXCLUDE
)
install(
    FILES
        ${DSE_MODELC_PUBLIC_HEADERS}
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/dse/modelc
    COMPONENT
        modelc
)
file(GLOB LICENSE_FILES ../../licenses/*.*)
install(
    FILES
        ${LICENSE_FILES}
    DESTINATION
        licenses
    COMPONENT
        modelc
)


# Sub Projects
# ============
add_subdirectory(tools/modelc)
add_subdirectory(tools/simbus)
add_subdirectory(tools/mcl_model)
add_subdirectory(tools/mstep)
add_subdirectory(examples)


# Package
# =======
set(CPACK_SYSTEM_NAME $ENV{PACKAGE_ARCH})
set(CPACK_PACKAGE_VENDOR "Robert Bosch GmbH")
set(CPACK_PACKAGE_DESCRIPTION "DSE Model C including: SimBus and ModelC")
set(CPACK_OUTPUT_FILE_PREFIX _dist)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_CHECKSUM MD5)

# Note: Component install is currently disabled, in order to get a single ZIP
#  distribution with examples included.
# set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
# get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
set(CPACK_ARCHIVE_MODELC_FILE_NAME "ModelC-$ENV{PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
# set(CPACK_ARCHIVE_DYNAMIC_MODEL_FILE_NAME "DynamicModel-$ENV{PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")

include(CPack)
