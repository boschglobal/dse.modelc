# Target: Linux x86

set(CMAKE_SYSTEM_NAME Linux)
set(CDEF_PLATFORM_OS linux)         # Available via dse/clib/platform.h.
set(CDEF_PLATFORM_ARCH x86)         # Available via dse/clib/platform.h.

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mx32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mx32")
