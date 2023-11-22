# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(dynamic)
add_subdirectory(binary)
add_subdirectory(gateway)
add_subdirectory(stacked)

# Code examples for documentation.
if(UNIX)
add_subdirectory(apis)
endif()
