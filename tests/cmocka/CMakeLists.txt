cmake_minimum_required(VERSION 3.21)

#set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_modelc)

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
    ${DSE_CLIB_SOURCE_DIR}/collections/hashmap.c
    ${DSE_CLIB_SOURCE_DIR}/collections/set.c
)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/../..")


# External Project - DSE Network Codec
# ------------------------------------
set(DSE_NCODEC_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/dse_ncodec")
set(DSE_NCODEC_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/dse_ncodec")
find_library(DSE_NCODEC_LIB
    NAMES
        libautomotive-bus-codec.a
    PATHS
        ${DSE_NCODEC_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(dse_ncodec STATIC IMPORTED GLOBAL)
set_target_properties(dse_ncodec
    PROPERTIES
        IMPORTED_LOCATION "${DSE_NCODEC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DSE_NCODEC_SOURCE_DIR}"
)
set(DSE_NCODEC_SOURCE_FILES
    ${DSE_NCODEC_SOURCE_DIR}/dse/ncodec/codec.c
)
set(DSE_NCODEC_INCLUDE_DIR "${DSE_NCODEC_SOURCE_DIR}")



# Set the project paths
# =====================
set(DSE_MODELC_SOURCE_DIR ../../dse/modelc)
set(DSE_MOCKS_SOURCE_DIR ../../dse/mocks)
set(DSE_MODELC_SOURCE_FILES
    ${DSE_MODELC_SOURCE_DIR}/model/gateway.c
    ${DSE_MODELC_SOURCE_DIR}/model/model.c
    ${DSE_MODELC_SOURCE_DIR}/model/ncodec.c
    ${DSE_MODELC_SOURCE_DIR}/model/schema.c
    ${DSE_MODELC_SOURCE_DIR}/model/signal.c
    ${DSE_MODELC_SOURCE_DIR}/model/trace.c

    ${DSE_MODELC_SOURCE_DIR}/controller/loader.c
    ${DSE_MODELC_SOURCE_DIR}/controller/model_function.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc_debug.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc_args.c
    ${DSE_MODELC_SOURCE_DIR}/controller/step.c
    ${DSE_MODELC_SOURCE_DIR}/controller/transform.c

    ${DSE_MOCKS_SOURCE_DIR}/simmock.c
)
set(DSE_MODELC_INCLUDE_DIR "${DSE_MODELC_SOURCE_DIR}/../..")



# Targets
# =======

# Target - Model
# ---------------
add_executable(test_model
    model/__test__.c
    model/test_gateway.c
    model/test_ncodec.c
    model/test_schema.c
    model/test_signal.c
    model/test_transform.c
    model/stub_controller.c
    ${DSE_MODELC_SOURCE_FILES}
    ${DSE_NCODEC_SOURCE_FILES}
    ${DSE_CLIB_SOURCE_FILES}
)
target_include_directories(test_model
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
        ./
)
target_compile_definitions(test_model
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(test_model
    PRIVATE
        dse_ncodec
        cmocka
        yaml
        dl
        m
        # -Wl,--wrap=strdup # Wrapping strdup does not work with libyaml.
)
install(TARGETS test_model)
install(
    FILES
        model/gateway.yaml
        model/ncodec.yaml
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
    model/stub_controller.c
    ${DSE_MODELC_SOURCE_FILES}
    ${DSE_NCODEC_SOURCE_FILES}
    ${DSE_CLIB_SOURCE_FILES}
)
target_include_directories(test_model_interface
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
        ./
)
target_compile_definitions(test_model_interface
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(test_model_interface
    PRIVATE
        dse_ncodec
        cmocka
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
