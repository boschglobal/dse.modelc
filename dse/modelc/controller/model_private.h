// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_CONTROLLER_MODEL_PRIVATE_H_
#define DSE_MODELC_CONTROLLER_MODEL_PRIVATE_H_


#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/mcl_mk1.h>


typedef struct ModelInstancePrivate {
    ControllerModel* controller_model;
    AdapterModel*    adapter_model;
    MclInstanceDesc* mcl_instance;
} ModelInstancePrivate;


#endif  // DSE_MODELC_CONTROLLER_MODEL_PRIVATE_H_
