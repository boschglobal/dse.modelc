// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/timer.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/controller/model_private.h>


#define UNUSED(x) ((void)x)


typedef struct mf_step_data {
    ModelInstanceSpec* mi;
    double             model_time;
    double             stop_time;
} mf_step_data;


static int _do_step_func(void* _mf, void* _step_data)
{
    ModelFunction* mf = _mf;
    mf_step_data*  step_data = _step_data;
    ModelDesc*     md = step_data->mi->model_desc;
    double         model_time = step_data->model_time;
    int            rc = md->vtable.step(md, &model_time, step_data->stop_time);
    if (rc)
        log_error(
            "Model Function %s:%s (rc=%d)", step_data->mi->name, mf->name, rc);

    return 0;
}


int step_model(ModelInstanceSpec* mi, double* model_time)
{
    ModelInstancePrivate* mip = mi->private;
    ControllerModel*      cm = mip->controller_model;
    AdapterModel*         am = mip->adapter_model;

    /* Step the Model (i.e. call registered Model Functions). */
    mf_step_data    step_data = { mi, am->model_time, am->stop_time };
    HashMap*        mf_map = &cm->model_functions;
    struct timespec stepcall_ts = get_timespec_now();
    int rc = hashmap_iterator(mf_map, _do_step_func, false, &step_data);
    am->bench_steptime_ns = get_elapsedtime_ns(stepcall_ts);

    /* Update the Model times. */
    am->model_time = am->stop_time;
    *model_time = am->model_time;
    return rc;
}


DLL_PRIVATE int sim_step_models(SimulationSpec* sim, double* model_time)
{
    assert(sim);
    int rc = 0;
    errno = 0;

    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        rc = step_model(_instptr, model_time);
        if (rc) break;
        /* Next instance? */
        _instptr++;
    }
    if (rc < 0) {
        log_error("An error occurred while in Step Handler");
        return 1;
    }
    if (rc > 0) {
        log_error("Model requested exit");
        return 2;
    }

    return 0;
}
