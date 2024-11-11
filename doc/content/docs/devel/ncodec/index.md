---
title: "Model C with Network Codec"
linkTitle: "Network Codec"
draft: false
tags:
- Developer
- ModelC
- NCodec
- Codec
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## Network Codec

The Model C Library integrates the [DSE Network Codec](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec) implementation of the [Automotive Bus schemas](https://github.com/boschglobal/automotive-bus-schema).





### Configuration of Binary Signals

> IMPORTANT: It is recommended to specify a SignalGroup for each individual Model Instance in a Simulation. This is so that the MIMEtype of each Binary Signal can be completely configured (especially `bus_id`,`node_id` and `interface_id`). Models _may_ implement supplemental configuration options (such as annotations on the Model Instance definition) which can further adjust or augment the MIMEtype parameters.


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


### Tracing NCodec Frames

The ModelC runtime can enable tracing for selected (or all) frames being sent or received by a Model Instance. This tracing is enabled via environment variables.


#### Example Trace

For the Binary Signal described by the following MIMEtype:

<pre>application/x-automotive-bus; interface=stream; type=frame; <b>bus=can</b>; schema=fbs; <b>bus_id=1</b>; node_id=2; interface_id=3</pre>


The ModelC command would be (setting the environment variable directly):

<pre>
NCODEC_TRACE_<b>CAN_1</b>=<b>0x3ea,0x3eb</b> modelc --name=ncodec_inst ...
</pre>

or

<pre>
NCODEC_TRACE_<b>CAN_1</b>=<b>*</b> modelc --name=ncodec_inst ...
</pre>


### Usage in Model Code

The Network Codec integration is fairly easy to use. The general approach is as follows:

{{< readfile file="../../examples/modelc/ncodec/ncodec_model.c" code="true" lang="c" >}}


For most use cases, the call sequence during a simulation step will be:

* **`ncodec_seek()`** - **Models typically do not call this method!** This call sets up the Binary Signal for reading and will be called automatically by the ModelC runtime.
* **`ncodec_read()`** - Models call `ncodec_read()` to read messages. Repeat calls until the function returns `-ENOMSG`.
* **`ncodec_truncate()`** - Models call `ncodec_truncate()` to prepare the Binary Signal for writing. **Mandatory**, call even if not intending to write frames to the NCodec in the current simulation step.
* **`ncodec_write()`** - Models call `ncodec_write()` to write messages. Can be called several times.
* **`ncodec_flush()`** - Models call `ncodec_flush()` to copy the buffered writes to the Binary Signal. Typically called at the end of a simulation step, however `ncodec_flush()` can be called many times; each time its called the accrued content from prior calls to `ncodec_write()` are appended to the Binary Signal.


### Usage in Test Cases

When developing Test Cases its possible to use an existing Network Codec object
to send or receive messages with the objects owner (usually a model). This is
particularly useful for testing messaging behaviour of a model under specific
circumstances. When doing this its necessary to circumvent the `node_id`
filtering of that codec, as the following example illustrates.

{{< readfile file="../../examples/modelc/ncodec/ncodec_test.c" code="true" lang="c" >}}


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
