# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(ModelC_Adapter)

include(FetchContent)


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



# Targets
# =======

# Target - adapter
# ----------------
add_library(adapter OBJECT
    adapter.c
    message.c
    simbus/adapter.c
    simbus/handler.c
    simbus/profile.c
    simbus/states.c
    transport/endpoint.c
#    transport/mq.c
#    transport/mq_posix.c
#    transport/mq_namedpipe.c
#    transport/redis.c
    transport/redispubsub.c
    ${DSE_SCHEMAS_SOURCE_DIR}/dse_schemas/flatcc/src/builder.c
    ${DSE_SCHEMAS_SOURCE_DIR}/dse_schemas/flatcc/src/emitter.c
    ${DSE_SCHEMAS_SOURCE_DIR}/dse_schemas/flatcc/src/refmap.c
)
target_include_directories(adapter
    PRIVATE
        ${HIREDIS_INCLUDE_DIR}
        ${MSGPACKC_SOURCE_DIR}/include
        ${EVENT_SOURCE_DIR}/include
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_SCHEMAS_SOURCE_DIR}
        ${DSE_SCHEMAS_SOURCE_DIR}/dse_schemas/flatcc/include
        ../../..
)
target_link_libraries(adapter
    PRIVATE
        msgpackc
        hiredis
        event
        m
        $<$<BOOL:${UNIX}>:rt>
        $<$<BOOL:${WIN32}>:ws2_32>
        $<$<BOOL:${WIN32}>:iphlpapi>
        $<$<AND:$<BOOL:${WIN32}>,$<STREQUAL:${CMAKE_CXX_COMPILER_ID},"GNU">>:"-static winpthread">

)
