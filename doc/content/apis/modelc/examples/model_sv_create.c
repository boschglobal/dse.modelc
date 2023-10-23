#include <dse/modelc/model.h>
#include <dse/logger.h>

void print_signal_names(ModelInstanceSpec* mi)
{
    SignalVector* sv_save = model_sv_create(mi);
    for (SignalVector* sv = sv_save; sv->name; sv++) {
        for (uint32_t i = 0; i < sv->count; i++) {
            log_debug("  signal : %s", sv->signal[i]);
        }
    }
    model_sv_destroy(sv_save);
}
