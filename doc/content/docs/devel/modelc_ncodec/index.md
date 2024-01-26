---
title: "Model C with Network Codec"
linkTitle: "ModelC NCodec"
draft: false
tags:
- Developer
- ModelC
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## Network Codec

The Model C Library integrates the [DSE Network Codec](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec) implementation of the [Automotive Bus schemas](https://github.com/boschglobal/automotive-bus-schema).


### Build Integration

The Network Codec integration repackages the necessary include files with the Model C Library packages. No additional build integration is required.
A typical CMake configuration might look like this:

```cmake
FetchContent_Declare(dse_modelc_lib
    URL                 ${MODELC_LIB__URL}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc.lib"
)
FetchContent_MakeAvailable(dse_modelc_lib)
set(DSE_MODELC_INCLUDE_DIR "${dse_modelc_lib_SOURCE_DIR}/include")

# ... dynamic linked Model (ModelC.exe provides objects) ...

target_include_directories(some_target
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
        ../..
)

# ... static linked target (typical CMocka test application) ...

set(MODELC_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc.lib")
find_library(MODELC_LIB
    NAMES
        libmodelc_bundled.a
    PATHS
        ${MODELC_BINARY_DIR}/lib
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(modelc STATIC IMPORTED GLOBAL)
set_target_properties(modelc
    PROPERTIES
        IMPORTED_LOCATION "${MODELC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${MODELC_BINARY_DIR}"
)
set(DSE_MODELC_LIB_INCLUDE_DIR "${MODELC_BINARY_DIR}/include")
add_executable(test_mstep
    mstep/__test__.c
    mstep/test_mstep.c
)
target_include_directories(test_mstep
    PRIVATE
        ${DSE_MODELC_LIB_INCLUDE_DIR}
        ./
)
target_link_libraries(test_mstep
    PUBLIC
        -Wl,-Bstatic modelc -Wl,-Bdynamic ${CMAKE_DL_LIBS}
    PRIVATE
        cmocka
        dl
        m
)
```


### Configuration of Binary Signals

Binary signals are configured with a MIME Type. The Signal Vector integration with the Network Codec will automatically open `codec` objects for each supported MIME Type that is supported (by the codec). Additional configuration of the `codec` objects can be done with the `ncodec_config()` API function.

```yaml
kind: SignalGroup
metadata:
  name: network
  labels:
    channel: network_vector
  annotations:
    vector_type: binary
spec:
  signals:
    - signal: can_bus
      annotations:
        mime_type: application/x-automotive-bus; interface=stream; type=frame; bus=can; schema=fbs; bus_id=1; node_id=2; interface_id=3
```

Additional configuration information is available [here](https://github.com/boschglobal/dse.standards/blob/main/dse/ncodec/libs/automotive-bus/README.md). Especially the behaviour of `bus_id`,`node_id`and `interface_id` configuration items are described.

Configuration items can also be set at runtime with the `ncodec_config()` API as the following example shows:

{{< readfile file="../../examples/modelc/ncodec/ncodec_config.c" code="true" lang="c" >}}


### Usage in Model Code

The Network Codec integration is fairly easy to use. The general approach is as follows:

{{< readfile file="../../examples/modelc/ncodec/ncodec_model.c" code="true" lang="c" >}}


### Usage in Test Cases

When developing Test Cases its possible to use an existing Network Codec object
to send or receive messages with the objects owner (usually a model). This is
particularly useful for testing messaging behaviour of a model under specific
circumstances. When doing this its necessary to circumvent the `node_id`
filtering of that codec, as the following example illustrates.

{{< readfile file="../../examples/modelc/ncodec/ncodec_test.c" code="true" lang="c" >}}
