# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(ModelC_Adapter)


# Targets
# =======

# Target - flatcc_runtime
# -----------------------
add_library(flatcc_runtime OBJECT
    ${DSE_SCHEMAS_SOURCE_DIR}/dse_schemas/flatcc/src/builder.c
    ${DSE_SCHEMAS_SOURCE_DIR}/dse_schemas/flatcc/src/emitter.c
    ${DSE_SCHEMAS_SOURCE_DIR}/dse_schemas/flatcc/src/refmap.c
)
target_include_directories(flatcc_runtime
    PRIVATE
        ${DSE_SCHEMAS_SOURCE_DIR}/dse_schemas/flatcc/include
)


# Target - adapter
# ----------------
add_library(adapter OBJECT
    adapter.c
    adapter_msg.c
    adapter_loopb.c
    create.c
    index.c
    message.c
    uid_index.c
    simbus/adapter.c
    simbus/handler.c
    simbus/profile.c
    simbus/states.c
    transport/endpoint.c
    transport/redis.c
    transport/redispubsub.c
    transport/stream.c
    transport/stream_posix.c
)
target_include_directories(adapter
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_SCHEMAS_SOURCE_DIR}
        ${DSE_SCHEMAS_SOURCE_DIR}/dse_schemas/flatcc/include
        ../../..
)
target_link_libraries(adapter
    PRIVATE
        dse_external_hiredis
        dse_external_event
        dse_external_lua
        dse_external_dl
        m
        $<$<BOOL:${UNIX}>:rt>
        $<$<BOOL:${WIN32}>:ws2_32>
        $<$<BOOL:${WIN32}>:iphlpapi>
        $<$<AND:$<BOOL:${WIN32}>,$<STREQUAL:${CMAKE_CXX_COMPILER_ID},"GNU">>:"-static winpthread">
        $<$<AND:$<BOOL:${WIN32}>,$<NOT:$<STREQUAL:${CMAKE_CXX_COMPILER_ID},"GNU">>>:-static pthread>
)
