# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)
if(TARGET dse_external_lua)
    return()
endif()

include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/dse_external.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/oss_repos.cmake)
if(DEFINED ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()

set(DSE_EXTERNAL_LUA_PREFIX "${CMAKE_BINARY_DIR}/_deps/dse_external_lua")
set(DSE_EXTERNAL_LUA_INSTALL_DIR "${DSE_EXTERNAL_LUA_PREFIX}/install")
set(DSE_EXTERNAL_LUA_LIB "${DSE_EXTERNAL_LUA_INSTALL_DIR}/lib/liblua.a")
set(DSE_EXTERNAL_LUA_INCLUDE_DIR "${DSE_EXTERNAL_LUA_INSTALL_DIR}/include")

dse_external_download(lua "${ExternalProject__LUA__URL}"
    "${ExternalProject__LUA__USERNAME}" "${ExternalProject__LUA__PASSWORD}" DSE_EXTERNAL_LUA_ARCHIVE)
ExternalProject_Add(dse_external_lua_build
    URL                 "${DSE_EXTERNAL_LUA_ARCHIVE}"
    URL_HASH            ${DSE_EXTERNAL_LUA_ARCHIVE_HASH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX              "${DSE_EXTERNAL_LUA_PREFIX}"
    INSTALL_DIR         "${DSE_EXTERNAL_LUA_INSTALL_DIR}"
    EXCLUDE_FROM_ALL    TRUE
    BUILD_IN_SOURCE     TRUE
    PATCH_COMMAND       ${CMAKE_COMMAND} -E copy
                        ${CMAKE_CURRENT_LIST_DIR}/lua_makefile
                        <SOURCE_DIR>/makefile
    CONFIGURE_COMMAND   ""
    BUILD_COMMAND       make
                        "CMAKE_C_FLAGS=${CMAKE_C_FLAGS} -fPIC"
                        CC=${CMAKE_C_COMPILER}
    INSTALL_COMMAND     ${CMAKE_COMMAND} -E make_directory <INSTALL_DIR>/lib <INSTALL_DIR>/include
            COMMAND     ${CMAKE_COMMAND} -E copy liblua.a <INSTALL_DIR>/lib
            COMMAND     ${CMAKE_COMMAND} -E copy lua.h luaconf.h lualib.h lauxlib.h <INSTALL_DIR>/include
    BUILD_BYPRODUCTS    "${DSE_EXTERNAL_LUA_LIB}"
)

# Imported targets require their include directories to exist at generate time.
file(MAKE_DIRECTORY "${DSE_EXTERNAL_LUA_INCLUDE_DIR}")
add_library(dse_external_lua STATIC IMPORTED GLOBAL)
set_target_properties(dse_external_lua
    PROPERTIES
        IMPORTED_LOCATION "${DSE_EXTERNAL_LUA_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DSE_EXTERNAL_LUA_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "m;${CMAKE_DL_LIBS}"
)
add_dependencies(dse_external_lua dse_external_lua_build)
