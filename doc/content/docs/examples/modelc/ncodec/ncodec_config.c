#include <string.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>

void configure_codec(ModelDesc* m, SignalVector* sv, uint32_t idx)
{
    NCODEC* nc = sv->codec(sv, idx);
    const char* node_id = NULL;
    for (int i = 0; i >= 0; i++) {
        NCodecConfigItem nci = ncodec_stat(nc, &i);
        if (strcmp(nci.name, "node_id") == 0) {
            node_id = nci.value;
            break;
        }
    }
    if (node_id == NULL) {
        node_id = model_instance_annotation(m, "node_id");
        if (node_id == NULL) log_fatal("No node_id configuration found!");
        ncodec_config(nc, (struct NCodecConfigItem){
                                 .name = "node_id",
                                 .value = node_id,
                             });
    }
}
