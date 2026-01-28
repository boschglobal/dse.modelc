# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# Example Models.
add_subdirectory(binary)
add_subdirectory(block)
add_subdirectory(csv)
add_subdirectory(extended)
add_subdirectory(gateway)
add_subdirectory(gdb)
add_subdirectory(minimal)
add_subdirectory(ncodec)
add_subdirectory(pdunet)
add_subdirectory(runtime)
add_subdirectory(sequential)
add_subdirectory(simer)
add_subdirectory(transform)
if(UNIX)
add_subdirectory(lua)
add_subdirectory(benchmark)
endif()

# Code examples for documentation.
if(UNIX)
add_subdirectory(doc)
endif()
