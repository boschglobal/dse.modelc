// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/controller/controller.h>


DLL_PRIVATE void controller_transform_to_model(
    ModelFunctionChannel* mfc, SignalMap* sm)
{
    if (mfc->signal_transform) {
        for (uint32_t si = 0; si < mfc->signal_count; si++) {
            if (mfc->signal_transform[si].linear.factor != 0) {
                // Linear transform: value * factor + offset
                mfc->signal_value_double[si] =
                    sm[si].signal->val *
                        mfc->signal_transform[si].linear.factor +
                    mfc->signal_transform[si].linear.offset;
            } else {
                // Disabled (ie. div 0), direct.
                mfc->signal_value_double[si] = sm[si].signal->val;
            }
        }
    } else {
        for (uint32_t si = 0; si < mfc->signal_count; si++) {
            mfc->signal_value_double[si] = sm[si].signal->val;
        }
    }
}


DLL_PRIVATE void controller_transform_from_model(
    ModelFunctionChannel* mfc, SignalMap* sm)
{
    if (mfc->signal_transform) {
        for (uint32_t si = 0; si < mfc->signal_count; si++) {
            if (mfc->signal_transform[si].linear.factor != 0) {
                // Linear transform: value * factor + offset
                sm[si].signal->val =
                    (mfc->signal_value_double[si] -
                        mfc->signal_transform[si].linear.offset) /
                    mfc->signal_transform[si].linear.factor;
            } else {
                // Disabled, direct.
                sm[si].signal->val = mfc->signal_value_double[si];
            }
        }
    } else {
        for (uint32_t si = 0; si < mfc->signal_count; si++) {
            sm[si].signal->val = mfc->signal_value_double[si];
        }
    }
}
