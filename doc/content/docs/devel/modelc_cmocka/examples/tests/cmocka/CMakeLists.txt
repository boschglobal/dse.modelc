cmake_minimum_required(VERSION 3.21)

project(test_project)

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

FetchContent_Declare(dse_clib
    URL         https://github.com/boschglobal/dse.clib/archive/refs/tags/v1.0.5.tar.gz
    SOURCE_DIR  "$ENV{EXTERNAL_BUILD_DIR}/dse.clib"
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse/clib)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/../..")

add_executable(test_target
    runnable/__test__.c
    runnable/test_foo.c
)
target_include_directories(test_target
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ./
)
target_compile_definitions(test_target
    PUBLIC
        CMOCKA_TESTING
)
target_link_libraries(test_target
    PRIVATE
        cmocka
        dl
        m
)
install(TARGETS test_target)
