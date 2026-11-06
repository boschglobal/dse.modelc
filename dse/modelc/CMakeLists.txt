# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(ModelC
    DESCRIPTION "DSE Model C including: SimBus and ModelC."
    HOMEPAGE_URL "${PROJECT_URL}"
)
set(VERSION "$ENV{PACKAGE_VERSION}")
set(PROJECT_VERSION ${VERSION})

include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/_out)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb")

# Enable profiling flags.
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")

list(APPEND C_CXX_WARNING_FLAGS
    -Wall
    -W
    -Wwrite-strings
    -Wno-missing-field-initializers
    -Wno-misleading-indentation
)
add_compile_options(${C_CXX_WARNING_FLAGS})
add_compile_definitions(DLL_BUILD)

include(FetchContent)

# External Projects (targets common to sub projects)
# =================
set(DSE_MODELC_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../extra/external)
include(${DSE_MODELC_EXTERNAL_DIR}/event.cmake)
include(${DSE_MODELC_EXTERNAL_DIR}/hiredis.cmake)
include(${DSE_MODELC_EXTERNAL_DIR}/yaml.cmake)
include(${DSE_MODELC_EXTERNAL_DIR}/dlfcnwin32.cmake)
include(${DSE_MODELC_EXTERNAL_DIR}/lua.cmake)


# External Project - DSE C Lib
# ----------------------------
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{DSE_CLIB_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_CLIB_URL_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse)
set(DSE_CLIB_INCLUDE_DIR ${DSE_CLIB_SOURCE_DIR}/..)


# External Project - DSE Schemas
# ------------------------------
FetchContent_Declare(dse_schemas
    URL                 $ENV{DSE_SCHEMA_URL}
    HTTP_USERNAME       $ENV{DSE_SCHEMA_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_SCHEMA_URL_TOKEN}
    SOURCE_SUBDIR       flatbuffers/c
)
FetchContent_MakeAvailable(dse_schemas)
set(DSE_SCHEMAS_SOURCE_DIR ${dse_schemas_SOURCE_DIR}/flatbuffers/c)


# External Project - DSE Network Codec
# ------------------------------------
FetchContent_Declare(dse_ncodec
    URL                 $ENV{DSE_NCODEC_URL}
    HTTP_USERNAME       $ENV{DSE_NCODEC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_NCODEC_URL_TOKEN}
    SOURCE_SUBDIR       dse
)
FetchContent_MakeAvailable(dse_ncodec)
set(DSE_NCODEC_SOURCE_DIR ${dse_ncodec_SOURCE_DIR}/dse/ncodec)
set(DSE_NCODEC_INCLUDE_DIR ${dse_ncodec_SOURCE_DIR})
set(DSE_PDUNET_SOURCE_DIR ${dse_ncodec_SOURCE_DIR}/dse/pdunet)
set(DSE_PDUNET_INCLUDE_DIR ${dse_ncodec_SOURCE_DIR})


# Targets
# =======
set(DSE_MOCK_DIR ${CMAKE_SOURCE_DIR}/../mocks)
set(DSE_MODELC_PUBLIC_HEADERS
    gateway.h
    mcl.h
    model.h
    runtime.h
    schema.h
)
set(DSE_LUA_MODEL_DIR "model/lua")
file(GLOB_RECURSE DSE_LUA_MODEL_FILES
    "${DSE_LUA_MODEL_DIR}/*.lua"
)
add_subdirectory(adapter)
add_subdirectory(controller)
add_subdirectory(model)
if(UNIX)
add_subdirectory(${DSE_MOCK_DIR} ${CMAKE_CURRENT_BINARY_DIR}/mocks)
endif()

# Target - clib_api (APIs Called from Models/ModelC)
# -----------------
add_library(clib_api OBJECT
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/hashmap.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/queue.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/set.c
    ${DSE_CLIB_SOURCE_DIR}/clib/data/marshal.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/binary.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/strings.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/yaml.c
)
target_compile_definitions(clib_api
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_include_directories(clib_api
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
)
target_link_libraries(clib_api
    PRIVATE
        dse_external_yaml
        dse_external_dl
        m
)

# Model C (static)
# -------
if(UNIX)
add_library(modelc_static_lib STATIC)
set_target_properties(modelc_static_lib
    PROPERTIES
        OUTPUT_NAME modelc
)
target_link_libraries(modelc_static_lib
    PUBLIC
        adapter
        clib_api
        controller
        ab-codec
        pdunet
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
add_library(modelc_shared_lib SHARED)
set_target_properties(modelc_shared_lib
    PROPERTIES
        OUTPUT_NAME modelc
)
target_include_directories(modelc_shared_lib
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
)
target_link_libraries(modelc_shared_lib
    PUBLIC
        adapter
        clib_api
        controller
        ab-codec
        pdunet
        model_api
)
if(NOT DEFINED modelc_link_lib)
    set(modelc_link_lib modelc_shared_lib)
endif()
set(modelc_shared_lib modelc_shared_lib)


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
        ${DSE_NCODEC_SOURCE_DIR}/codec.h
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/dse/ncodec
    COMPONENT
        modelc
)
install(
    FILES
        ${DSE_NCODEC_SOURCE_DIR}/interface/frame.h
        ${DSE_NCODEC_SOURCE_DIR}/interface/pdu.h
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/dse/ncodec/interface
    COMPONENT
        modelc
)
install(
    FILES
        ${DSE_NCODEC_SOURCE_DIR}/stream/stream.h
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/dse/ncodec/stream
    COMPONENT
        modelc
)
install(
    FILES
        ${DSE_MODELC_PUBLIC_HEADERS}
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/dse/modelc
    COMPONENT
        modelc
)
install(
    DIRECTORY
        ${DSE_LUA_MODEL_DIR}/
    DESTINATION
        ${CMAKE_INSTALL_DATADIR}/lua/5.4
    COMPONENT
        modelc
)
install(
    DIRECTORY
        ../../licenses
    DESTINATION
        licenses
    COMPONENT
        modelc
)
install(
    FILES
        ${CMAKE_BINARY_DIR}/compile_commands.json
    DESTINATION
        doc
    COMPONENT
        modelc
)
if(UNIX)
install(
    FILES
        ${DSE_MOCK_DIR}/simmock.h
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/dse/mocks
    COMPONENT
        modelc
)
install(
    FILES
        ${DSE_MOCK_DIR}/simmock.c
    DESTINATION
        mocks
    COMPONENT
        modelc
)
endif()

# Sub Projects
# ============
add_subdirectory(tools/modelc)
add_subdirectory(tools/simbus)
add_subdirectory(tools/mstep)
add_subdirectory(examples)


# Package
# =======
set(CPACK_SYSTEM_NAME $ENV{PACKAGE_ARCH})
set(CPACK_PACKAGE_VENDOR "Robert Bosch GmbH")
set(CPACK_PACKAGE_DESCRIPTION "DSE Model C including: SimBus and ModelC")
set(CPACK_PACKAGE_VERSION $ENV{PACKAGE_VERSION})
set(CPACK_OUTPUT_FILE_PREFIX _dist)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_CHECKSUM MD5)
set(CPACK_PACKAGE_FILE_NAME "ModelC-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")

include(CPack)
