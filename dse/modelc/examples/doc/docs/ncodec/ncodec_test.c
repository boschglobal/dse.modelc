#include <dse/testing.h>
#include <dse/modelc/runtime.h>
#include <dse/ncodec/codec.h>

typedef struct ModelCMock {
    SimulationSpec     sim;
    ModelInstanceSpec* mi;
} ModelCMock;

char* get_ncodec_node_id(NCODEC* nc)
{
    assert_non_null(nc);
    int         index = 0;
    const char* node_id = NULL;
    while (index >= 0) {
        NCodecConfigItem ci = ncodec_stat(nc, &index);
        if (strcmp(ci.name, "node_id") == 0) {
            node_id = ci.value;
            break;
        }
        index++;
    }
    if (node_id) return strdup(node_id);
    return NULL;
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
    ModelCMock*   mock = *state;
    SignalVector* sv = mock->mi->model_desc->sv;
    NCODEC*       nc = sv->codec(sv, 2);
    const char*   buffer = "hello world";

    // ...

    // Modify the node_id.
    char* node_id_save = get_ncodec_node_id(nc);
    set_ncodec_node_id(nc, "42");
    // Send a message (which will not be filtered).
    ncodec_write(nc, &(struct NCodecCanMessage){
                         .frame_id = 42,
                         .buffer = (uint8_t*)buffer,
                         .len = strlen(buffer),
                     });
    ncodec_flush(nc);
    // Restore the existing node_id.
    set_ncodec_node_id(nc, node_id_save);
    free(node_id_save);

    // ...
}
