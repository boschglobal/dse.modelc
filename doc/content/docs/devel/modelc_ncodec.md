---
title: "Model C with Network Codec"
linkTitle: "ModelC NCodec"
draft: false
tags:
- Developer
- ModelC
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
path_base_for_github_subdir: "content/docs/devel/"
---

## Network Codec

The Model C Library integrates the [DSE Network Codec](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec) implementation of the [Automotive Bus schemas](https://github.com/boschglobal/automotive-bus-schema).


### Build Integration

The Network Codec integration repackages the necessary include files with the Model C Library packages. No additional build integration is required.


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