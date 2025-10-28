# Target: Linux i386

set(CMAKE_SYSTEM_NAME Linux)
set(CDEF_PLATFORM_OS linux)         # Available via dse/clib/platform.h.
set(CDEF_PLATFORM_ARCH i386)         # Available via dse/clib/platform.h.

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
