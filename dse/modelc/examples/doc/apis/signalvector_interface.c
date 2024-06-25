#include <string.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>

#define UNUSED(x) ((void)x)

int model_step(ModelDesc* m, double* model_time, double stop_time)
{
    SignalVector* sv = m->sv;
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
                sv->vtable.reset(sv, i);
                sv->vtable.append(sv, i, data, strlen("foo"));
                free(data);
                sv->vtable.release(sv, i);
                const char* mime_type = sv->vtable.annotation(sv, i, "mime_type");
                if (mime_type) log_debug("    annotation : %s", mime_type);
            } else {
                log_debug("  scalar : %s", sv->scalar[i]);
            }
        }

        // Next signal vector.
        sv++;
    }

    *model_time = stop_time;
    return 0;
}
