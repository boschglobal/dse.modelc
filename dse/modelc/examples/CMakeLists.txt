# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# Example Models.
add_subdirectory(minimal)
add_subdirectory(extended)
add_subdirectory(binary)
add_subdirectory(ncodec)
add_subdirectory(simer)
add_subdirectory(gateway)
add_subdirectory(gdb)
if(UNIX)
add_subdirectory(benchmark)
endif()

# Code examples for documentation.
if(UNIX)
add_subdirectory(doc)
endif()
