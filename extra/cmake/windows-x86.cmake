# Target: Windows x64

set(CMAKE_SYSTEM_NAME Windows)
set(CDEF_PLATFORM_OS windows)       # Available via dse/clib/platform.h.
set(CDEF_PLATFORM_ARCH x86)         # Available via dse/clib/platform.h.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")

set(TOOLCHAIN_PREFIX i686-w64-mingw32)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_PREFIX}-gfortran)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

add_compile_definitions(SIGPIPE=13)

# Enable CCache if available.
# Trigger secondary storage by setting environment variables:
#   CCACHE_SECONDARY_STORAGE=redis://YOUR_SERVER
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()
