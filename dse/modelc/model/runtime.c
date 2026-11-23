// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/modelc/model.h>


#define UNUSED(x) ((void)x)


DLL_PRIVATE int __model_vm_step__(
    ModelDesc* model, double* model_time, double stop_time)
{
    UNUSED(model);
    *model_time = stop_time;
    return 0;
}


DLL_PRIVATE ModelDesc* __model_vm_create__(ModelDesc* model)
{
    return model;
}


DLL_PRIVATE void __model_vm_destroy__(ModelDesc* model)
{
    UNUSED(model);
}
