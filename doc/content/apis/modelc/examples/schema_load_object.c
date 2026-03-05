#include <stddef.h>
#include <stdint.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/schema.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define S             SchemaFieldTypeS
#define U8            SchemaFieldTypeU8
#define U32           SchemaFieldTypeU32

typedef enum {
    DirectionNone = 0,
    DirectionRx = 1, /* "Rx" */
    DirectionTx = 2, /* "Tx" */
} Direction;

typedef struct Signal {
    const char* name;
    uint32_t    id;
    struct {
        Direction dir;
    } metadata;
} Signal;

Signal object_load(YamlNode* n)
{
    Signal sig = {};

    static const SchemaFieldMapSpec dir_map[] = {
        { "Rx", DirectionRx },
        { "Tx", DirectionTx },
        { NULL },
    };
    static const SchemaFieldSpec sig_spec[] = {
        // clang-format off
        { S, "name", offsetof(Signal, name) },
        { U32, "id", offsetof(Signal, id) },
        { U8, "direction", offsetof(Signal, metadata.dir), dir_map },
        // clang-format on
    };
    schema_load_object(n, &sig, sig_spec, ARRAY_SIZE(sig_spec));

    return sig;
}