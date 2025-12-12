// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/logger.h>
#include <dse/testing.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/adapter/simbus/simbus.h>
#include <dse/modelc/model.h>
#include <mock.h>


int mock_setup(ModelCMock* m)
{
    int rc;
    //__log_level__ = LOG_DEBUG;

    modelc_set_default_args(&m->args, "test", 0.005, 0.005);
    m->args.log_level = __log_level__;
    modelc_parse_arguments(&m->args, m->argc, m->argv, m->model_name);
    rc = modelc_configure(&m->args, &m->sim);
    assert_int_equal(rc, 0);

    // Setup the controller (replaces call to modelc_run()).
    m->endpoint = endpoint_create(
        m->sim.transport, m->sim.uri, m->sim.uid, false, m->sim.timeout);
    controller_init(m->endpoint, &m->sim);
    m->controller = controller_object_ref(&m->sim);
    m->controller->simulation = &m->sim;

    // Locate the model instance and setup adapter objects.
    m->mi = modelc_get_model_instance(&m->sim, m->args.name);
    assert_non_null(m->mi);
    ModelInstancePrivate* mip = m->mi->private;
    AdapterModel*         am = mip->adapter_model;
    am->adapter = m->controller->adapter;
    am->model_uid = m->mi->uid;

    // Mock the model VTable (replaces call to controller_load_model()).
    rc = modelc_model_create(&m->sim, m->mi, &m->vtable);
    assert_int_equal(rc, 0);

    // Push the controller to ready state.
    controller_bus_ready(&m->sim);

    return 0;
}


int mock_teardown(ModelCMock* m)
{
    if (m == NULL) return 0;

    void* save_doc_list = NULL;
    if (m->mi) save_doc_list = m->mi->yaml_doc_list;
    modelc_exit(&m->sim);
    if (save_doc_list) dse_yaml_destroy_doc_list(save_doc_list);

    return 0;
}
