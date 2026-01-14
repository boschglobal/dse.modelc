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


# External Project - lua
# ----------------------
if(UNIX)
set(LUA_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/lua")
set(LUA_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/lua")
find_library(LUA_LIB
    NAMES
        liblua.a
    PATHS
        ${LUA_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(lua STATIC IMPORTED GLOBAL)
set_target_properties(lua
    PROPERTIES
        IMPORTED_LOCATION "${LUA_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${LUA_BINARY_DIR}"
)
endif()


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
    SOURCE_SUBDIR       dse/ncodec
)
FetchContent_MakeAvailable(dse_ncodec)
set(DSE_NCODEC_SOURCE_DIR ${dse_ncodec_SOURCE_DIR}/dse/ncodec)
set(DSE_NCODEC_INCLUDE_DIR ${dse_ncodec_SOURCE_DIR})


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
    ${DSE_MODELC_SOURCE_DIR}/model/ncodec.c
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
    ${DSE_MOCKS_SOURCE_DIR}/simmock.c
)



set(DSE_MODELC_SIMBUS_LOOPBACK_SOURCE_FILES
    ${DSE_MODELC_SOURCE_DIR}/model/gateway.c
    ${DSE_MODELC_SOURCE_DIR}/model/lua.c
    ${DSE_MODELC_SOURCE_DIR}/model/luamodel.c
    ${DSE_MODELC_SOURCE_DIR}/model/mcl.c
    ${DSE_MODELC_SOURCE_DIR}/model/model.c
    ${DSE_MODELC_SOURCE_DIR}/model/ncodec.c
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
        ${YAML_SOURCE_DIR}/include
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
        ab-codec
    PRIVATE
        cmocka
        lua
        yaml
        dl
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


# Target - Model Interface
# ------------------------
add_executable(test_model_interface
    model/interface/__test__.c
    model/interface/test_model_api.c
    model/interface/test_model_examples.c
    ${DSE_CLIB_SOURCE_FILES}
    ${DSE_MODELC_SOURCE_FILES}
)
target_include_directories(test_model_interface
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
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
        ab-codec
    PRIVATE
        cmocka
        lua
        yaml
        dl
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
    simbus/test_direct_index.c
    simbus/test_map_index.c
    ${DSE_CLIB_SOURCE_FILES}
    ${DSE_MODELC_SIMBUS_LOOPBACK_SOURCE_FILES}
)
target_include_directories(test_simbus_loopback
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
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
        ab-codec
    PRIVATE
        cmocka
        lua
        yaml
        dl
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
