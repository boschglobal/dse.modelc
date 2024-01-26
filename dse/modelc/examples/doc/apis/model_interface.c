#include <errno.h>
#include <stddef.h>
#include <dse/modelc/model.h>

#define UNUSED(x) ((void)x)

ModelDesc* model_create(ModelDesc* m)
{
    return (ModelDesc*)m;
}

int model_step(ModelDesc* m, double* model_time, double stop_time)
{
    ModelSignalIndex counter = m->index(m, "data", "counter");
    if (counter.scalar == NULL) return -EINVAL;
    *(counter.scalar) += 1;

    *model_time = stop_time;
    return 0;
}

void model_destroy(ModelDesc* m)
{
    UNUSED(m);
}
