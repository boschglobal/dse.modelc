cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

#set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_modelc)
set(VERSION "$ENV{PACKAGE_VERSION}")
set(PROJECT_VERSION ${VERSION})

include(FetchContent)
include(GNUInstallDirs)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/_out)

set(CMAKE_ENABLE_EXPORTS ON)

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


# External Projects
# =================
set(DSE_MODELC_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../extra/external)
include(${DSE_MODELC_EXTERNAL_DIR}/yaml.cmake)
include(${DSE_MODELC_EXTERNAL_DIR}/dlfcnwin32.cmake)
include(${DSE_MODELC_EXTERNAL_DIR}/lua.cmake)


# External Project - DSE C Lib
# ----------------------------
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{DSE_CLIB_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_CLIB_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.clib"
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse/clib)
set(DSE_CLIB_SOURCE_FILES
    ${DSE_CLIB_SOURCE_DIR}/util/yaml.c
    ${DSE_CLIB_SOURCE_DIR}/util/strings.c
    ${DSE_CLIB_SOURCE_DIR}/util/binary.c  # FIXME still use this API?
    ${DSE_CLIB_SOURCE_DIR}/data/marshal.c
    ${DSE_CLIB_SOURCE_DIR}/collections/hashmap.c
    ${DSE_CLIB_SOURCE_DIR}/collections/set.c
)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/../..")


# External Project - DSE Network Codec
# ------------------------------------
FetchContent_Declare(dse_ncodec
    URL                 $ENV{DSE_NCODEC_URL}
    HTTP_USERNAME       $ENV{DSE_NCODEC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_NCODEC_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.ncodec"
    SOURCE_SUBDIR       dse
)
FetchContent_MakeAvailable(dse_ncodec)
set(DSE_NCODEC_SOURCE_DIR ${dse_ncodec_SOURCE_DIR}/dse/ncodec)
set(DSE_NCODEC_INCLUDE_DIR ${dse_ncodec_SOURCE_DIR})
set(DSE_PDUNET_SOURCE_DIR ${dse_ncodec_SOURCE_DIR}/dse/pdunet)
set(DSE_PDUNET_INCLUDE_DIR ${dse_ncodec_SOURCE_DIR})


# Set the project paths
# =====================
set(DSE_MODELC_SOURCE_DIR ../../dse/modelc)
set(DSE_MOCKS_SOURCE_DIR ../../dse/mocks)
set(DSE_MODELC_SOURCE_FILES
    ${DSE_MODELC_SOURCE_DIR}/model/gateway.c
    ${DSE_MODELC_SOURCE_DIR}/model/lua.c
    ${DSE_MODELC_SOURCE_DIR}/model/luamodel.c
    ${DSE_MODELC_SOURCE_DIR}/model/mcl.c
    ${DSE_MODELC_SOURCE_DIR}/model/model.c
    ${DSE_MODELC_SOURCE_DIR}/model/runtime.c
    ${DSE_MODELC_SOURCE_DIR}/model/ncodec.c
    ${DSE_MODELC_SOURCE_DIR}/model/pdunet.c
    ${DSE_MODELC_SOURCE_DIR}/model/schema.c
    ${DSE_MODELC_SOURCE_DIR}/model/signal.c
    ${DSE_MODELC_SOURCE_DIR}/model/trace.c

    ${DSE_MODELC_SOURCE_DIR}/controller/controller_stub.c
    ${DSE_MODELC_SOURCE_DIR}/controller/loader.c
    ${DSE_MODELC_SOURCE_DIR}/controller/log.c
    ${DSE_MODELC_SOURCE_DIR}/controller/model_function.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc_debug.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc_args.c
    ${DSE_MODELC_SOURCE_DIR}/controller/step.c
    ${DSE_MODELC_SOURCE_DIR}/controller/transform.c

    ${DSE_MODELC_SOURCE_DIR}/adapter/index.c
    ${DSE_MODELC_SOURCE_DIR}/adapter/uid_index.c
    ${DSE_MOCKS_SOURCE_DIR}/simmock.c
)



set(DSE_MODELC_SIMBUS_LOOPBACK_SOURCE_FILES
    ${DSE_MODELC_SOURCE_DIR}/model/gateway.c
    ${DSE_MODELC_SOURCE_DIR}/model/lua.c
    ${DSE_MODELC_SOURCE_DIR}/model/luamodel.c
    ${DSE_MODELC_SOURCE_DIR}/model/mcl.c
    ${DSE_MODELC_SOURCE_DIR}/model/model.c
    ${DSE_MODELC_SOURCE_DIR}/model/runtime.c
    ${DSE_MODELC_SOURCE_DIR}/model/ncodec.c
    ${DSE_MODELC_SOURCE_DIR}/model/pdunet.c
    ${DSE_MODELC_SOURCE_DIR}/model/schema.c
    ${DSE_MODELC_SOURCE_DIR}/model/signal.c
    ${DSE_MODELC_SOURCE_DIR}/model/trace.c

    ${DSE_MODELC_SOURCE_DIR}/controller/controller.c
    ${DSE_MODELC_SOURCE_DIR}/controller/loader.c
    ${DSE_MODELC_SOURCE_DIR}/controller/log.c
    ${DSE_MODELC_SOURCE_DIR}/controller/model_function.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc_debug.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc_args.c
    ${DSE_MODELC_SOURCE_DIR}/controller/step.c
    ${DSE_MODELC_SOURCE_DIR}/controller/transform.c

    ${DSE_MODELC_SOURCE_DIR}/adapter/adapter.c
    ${DSE_MODELC_SOURCE_DIR}/adapter/adapter_loopb.c
    ${DSE_MODELC_SOURCE_DIR}/adapter/create.c
    ${DSE_MODELC_SOURCE_DIR}/adapter/index.c
    ${DSE_MODELC_SOURCE_DIR}/adapter/uid_index.c

    ${DSE_MOCKS_SOURCE_DIR}/adaptermock.c
)
set(DSE_MODELC_INCLUDE_DIR "${DSE_MODELC_SOURCE_DIR}/../..")



# Targets
# =======

# Target - ModelC Stub
# --------------------
add_library(modelc_stub SHARED
    ${DSE_MOCKS_SOURCE_DIR}/modelc_stub.c
)
set_target_properties(modelc_stub
    PROPERTIES
        OUTPUT_NAME modelc
)
install(TARGETS modelc_stub)


# Target - Model
# ---------------
add_executable(test_model
    model/__test__.c
    model/test_gateway.c
    model/test_ncodec_can.c
    model/test_ncodec_pdu.c
    model/test_schema.c
    model/test_stack.c
    model/test_signal.c
    model/test_transform.c
    ${DSE_CLIB_SOURCE_FILES}
    ${DSE_MODELC_SOURCE_FILES}
)
target_include_directories(test_model
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ./
)
target_compile_definitions(test_model
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(test_model
    PUBLIC
        pdunet
        ab-codec
    PRIVATE
        cmocka
        dse_external_lua
        dse_external_yaml
        dse_external_dl
        m
        # -Wl,--wrap=strdup # Wrapping strdup does not work with libyaml.
)
install(TARGETS test_model)
install(
    FILES
        model/gateway.yaml
        model/internal.yaml
        model/ncodec.yaml
        model/sequential.yaml
        model/signal.yaml
        model/transform.yaml
    DESTINATION
        resources/model
)
set(FLATCC_INCLUDE_DIR ${DSE_NCODEC_SOURCE_DIR}/schema/abs/flatcc/include)


# Target - Model Interface
# ------------------------
add_executable(test_model_interface
    model/interface/__test__.c
    model/interface/test_model_api.c
    model/interface/test_model_examples.c
    model/interface/test_pdunet_ncodec.c
    ${DSE_CLIB_SOURCE_FILES}
    ${DSE_MODELC_SOURCE_FILES}
)
target_include_directories(test_model_interface
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${FLATCC_INCLUDE_DIR}
        ./
)
target_compile_definitions(test_model_interface
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(test_model_interface
    PUBLIC
        pdunet
        ab-codec
    PRIVATE
        cmocka
        dse_external_lua
        dse_external_yaml
        dse_external_dl
        m
        # -Wl,--wrap=strdup # Wrapping strdup does not work with libyaml.
)
install(TARGETS test_model_interface)
install(
    FILES
        model/interface/annotations.yaml
        model/interface/binary_stream_reset.yaml
    DESTINATION
        resources/model
)


# Target - SimBus
# ---------------
add_executable(test_simbus_loopback
    simbus/__test__.c
    simbus/mock.c
    simbus/test_uid_index.c
    ${DSE_CLIB_SOURCE_FILES}
    ${DSE_MODELC_SIMBUS_LOOPBACK_SOURCE_FILES}
)
target_include_directories(test_simbus_loopback
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ./simbus
)
target_compile_definitions(test_simbus_loopback
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(test_simbus_loopback
    PUBLIC
        pdunet
        ab-codec
    PRIVATE
        cmocka
        dse_external_lua
        dse_external_yaml
        dse_external_dl
        m
        # -Wl,--wrap=strdup # Wrapping strdup does not work with libyaml.
)
install(TARGETS test_simbus_loopback)
install(
    FILES
        simbus/direct_index.yaml
        simbus/map_index.yaml
    DESTINATION
        resources/simbus
)


# Target - Model Runtime
# ----------------------
add_library(cmocka_runtime_model SHARED
    runtime/scalar/model.c
)
set_target_properties(cmocka_runtime_model
    PROPERTIES
        OUTPUT_NAME runtime_scalar
)
target_include_directories(cmocka_runtime_model
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
)
install(TARGETS cmocka_runtime_model
    LIBRARY DESTINATION resources/runtime/scalar/lib
    RUNTIME DESTINATION resources/runtime/scalar/lib
)

add_library(cmocka_runtime_binary_model SHARED
    runtime/binary/model.c
)
set_target_properties(cmocka_runtime_binary_model
    PROPERTIES
        OUTPUT_NAME runtime_binary
)
target_include_directories(cmocka_runtime_binary_model
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
)
install(TARGETS cmocka_runtime_binary_model
    LIBRARY DESTINATION resources/runtime/binary/lib
    RUNTIME DESTINATION resources/runtime/binary/lib
)

add_library(cmocka_runtime_ncodec_model SHARED
    runtime/ncodec/model.c
)
set_target_properties(cmocka_runtime_ncodec_model
    PROPERTIES
        OUTPUT_NAME runtime_ncodec
)
target_include_directories(cmocka_runtime_ncodec_model
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
)
install(TARGETS cmocka_runtime_ncodec_model
    LIBRARY DESTINATION resources/runtime/ncodec/lib
    RUNTIME DESTINATION resources/runtime/ncodec/lib
)

add_executable(test_runtime
    runtime/__test__.c
    runtime/test_runtime.c
    ${DSE_CLIB_SOURCE_FILES}
    ${DSE_MODELC_SIMBUS_LOOPBACK_SOURCE_FILES}
    ${DSE_MODELC_SOURCE_DIR}/controller/model_runtime.c
)
target_include_directories(test_runtime
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ./
)
target_compile_definitions(test_runtime
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(test_runtime
    PUBLIC
        pdunet
        ab-codec
    PRIVATE
        cmocka
        dse_external_lua
        dse_external_yaml
        dse_external_dl
        m
)
install(TARGETS test_runtime)
install(
    FILES
        runtime/scalar/simulation.yaml
        runtime/scalar/model.yaml
        runtime/scalar/signals.yaml
    DESTINATION resources/runtime/scalar
)
install(
    FILES
        runtime/binary/simulation.yaml
        runtime/binary/model.yaml
        runtime/binary/signals.yaml
    DESTINATION resources/runtime/binary
)
install(
    FILES
        runtime/ncodec/simulation.yaml
        runtime/ncodec/model.yaml
        runtime/ncodec/signals.yaml
    DESTINATION resources/runtime/ncodec
)
