#include <dse/modelc/model.h>
#include <dse/modelc/schema.h>
#include <dse/logger.h>

int match_handler(ModelInstanceSpec* mi, SchemaObject* object)
{
    uint32_t            index = 0;
    SchemaSignalObject* so;
    do {
        so = schema_object_enumerator(
            mi, object, "spec/signals", &index, schema_signal_object_generator);
        if (so == NULL) break;
        if (so->signal) {
            log_debug("  signal identified: %s", so->signal);
        }
        free(so);
    } while (1);

    return 0;
}

void object_search(ModelInstanceSpec* mi, const char* channel_name)
{
    ChannelSpec           channel_spec = { .name = channel_name };
    SchemaObjectSelector* selector =
        schema_build_channel_selector(mi, &channel_spec, "SignalGroup");
    if (selector) {
        schema_object_search(mi, selector, match_handler);
    }
    schema_release_selector(selector);
}