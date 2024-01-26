#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>

void _setup_node_id(SignalVector* sv, uint32_t idx)
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