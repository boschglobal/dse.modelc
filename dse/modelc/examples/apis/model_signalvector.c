#include <dse/modelc/model.h>
#include <dse/logger.h>

void print_signal_vectors(ModelInstanceSpec* mi)
{
    SignalVector* sv = model_sv_create(mi);
    while (sv && sv->name) {
        log_debug("Signal Vector : %s", sv->name);

        for (uint32_t i = 0; i < sv->count; i++) {
            log_debug("  signal : %s", sv->signal[i]);
            if (sv->is_binary) {
                log_debug("    length : %s", sv->length[i]);
                log_debug("    buffer_size : %s", sv->buffer_size[i]);
                log_debug("    mime_type : %s", sv->mime_type[i]);

                // Example use of object functions.
                void* data = strdup("foo");
                sv->reset(sv, i);
                sv->append(sv, i, data, strlen("foo"));
                free(data);
                sv->release(sv, i);
                const char* mime_type = sv->annotation(sv, i, "mime_type");
                if (mime_type) log_debug("    annotation : %s", mime_type);
            } else {
                log_debug("  scalar : %s", sv->scalar[i]);
            }
        }

        // Next signal vector.
        sv++;
    }
}