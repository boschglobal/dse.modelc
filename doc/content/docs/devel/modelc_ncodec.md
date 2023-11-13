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

```c
#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>


static void _setup_node_id(SignalVector* sv, uint32_t idx)
{
    const char* v = sv->annotation(sv, idx, "node_id");
    if (v) {
        NCODEC* nc = sv->codec(sv, idx);
        ncodec_config(nc, (struct NCodecConfigItem){
            .name = "node_id",
            .value = v,
        });
    }
}
```

### Usage in Model Code

The Network Codec integration is fairly easy to use. The general approach is as follows:

```c
#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>


void do_bus_rx(SignalVector* sv, uint32_t idx)
{
    NCODEC* nc = sv->codec(sv, idx);

    while (1) {
        NCodecMessage msg = {};
        len = ncodec_read(nc, &msg);
        if (len < 0) break;
        put_rx_frame_to_queue(msg.frame_id, msg.buffer, msg.len);
    }
}

void do_bus_tx(SignalVector* sv, uint32_t idx)
{
    uint32_t id;
    uint8_t* msg;
    size_t len;
    NCODEC* nc = sv->codec(sv, idx);

    while (get_tx_frame_from_queue(&id, &msg, &len)) {
        ncodec_write(nc, &(struct NCodecMessage){
            .frame_id = id,
            .buffer = msg,
            .len = len,
        });
    }
    ncodec_flush(nc);
}

```

### Usage in Test Cases

When developing Test Cases its possible to use an existing Network Codec object
to send or receive messages with the objects owner (usually a model). This is
particularly useful for testing messaging behaviour of a model under specific
circumstances. When doing this its necessary to circumvent the `node_id`
filtering of that codec, as the following example illustrates.

```c
#include <dse/testing.h>
#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>


const char* get_ncodec_node_id(NCODEC* nc)
{
    assert_non_null(nc);
    int index = 0;
    const char* node_id = NULL;
    while (index >= 0) {
        NCodecConfigItem ci = ncodec_stat(nc, &index);
        if (strcmp(ci.name, "node_id") == 0) {
            node_id = ci.value;
            break;
        }
        index++;
    }
    return node_id;
}

void set_ncodec_node_id(NCODEC* nc, const char* node_id)
{
    assert_non_null(nc);
    ncodec_config(nc, (struct NCodecConfigItem){
        .name = "node_id",
        .value = node_id,
    });
}

void test_message_sequence(void** state)
{
    ModelCMock* mock = *state;
    NCODEC*     nc = sv->codec(sv, idx);

    // ...

    // Modify the node_id.
    const char* node_id_save = get_ncodec_node_id(nc);
    set_ncodec_node_id(nc, "42");
    // Send a message (which will not be filtered).
    ncodec_write(nc, &(struct NCodecMessage){
        .frame_id = frame_id,
        .buffer = data,
        .len = len,
    });
    ncodec_flush(nc);
    // Restore the existing node_id.
    set_ncodec_node_id(nc, node_id_save);

    // ...
}

```