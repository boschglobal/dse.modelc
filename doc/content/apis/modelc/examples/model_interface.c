#include <dse/modelc/model.h>
#include <dse/logger.h>


#define UNUSED(x) ((void)x)


/* Signal Vector definition.
   Note: Signal order should match order in related SignalGroup (YAML). */
typedef enum signal_name_index {
    SIGNAL_FOO,
    SIGNAL_BAR,
    __SIGNAL__COUNT__
} signal_name_index;

static double* signal_value;


int model_step(double* model_time, double stop_time)
{
    signal_value[SIGNAL_FOO] += 1.2;
    signal_value[SIGNAL_BAR] += 4.2;

    *model_time = stop_time;
    return 0;
}

int model_setup(ModelInstanceSpec* mi)
{
    int rc = model_function_register(mi, "example",
        0.005, model_step);
    if (rc != 0) return rc;

    /* Register channels (and get storage). */
    static ModelChannelDesc channel_desc = {
        .name = "model_channel",
        .function_name = "example",
    };
    rc = model_configure_channel(mi, &channel_desc);
    if (rc != 0) return rc;
    signal_value = channel_desc.vector_double;

    return 0;
}

int model_exit(ModelInstanceSpec* mi)
{
    UNUSED(mi);
    return 0;
}
