// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdlib.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/model/lua.h>

static inline double _call_lua_transform(
    lua_State* L, int32_t func_ref, double val)
{
    double ret_val = val;
    if ((func_ref <= 0) || (L == NULL)) return ret_val;

    lua_push_ctx(L);
    if (lua_ctx_set_double(L, "value", ret_val) != 0) goto cleanup;

    int rc = lua_call_ctx(L, func_ref);
    if (rc < 0) goto cleanup;
    if (rc == 0) goto cleanup;
    if (lua_ctx_get_double(L, "value", &ret_val) != 0) goto cleanup;

cleanup:
    lua_pop_ctx(L);
    return ret_val;
}


DLL_PRIVATE void model_function_channel_build_flat_sync(
    ModelFunctionChannel* mfc, SignalMap* sm)
{
    if (mfc == NULL || sm == NULL || mfc->signal_count == 0 ||
        mfc->signal_value_double == NULL) {
        model_function_channel_free_flat_sync(mfc);
        return;
    }

    if (mfc->signal_transform != NULL) {
        model_function_channel_free_flat_sync(mfc);
        mfc->scalar_sync.enabled = false;
        return;
    }

    if (mfc->scalar_sync.simbus_indices == NULL ||
        mfc->scalar_sync.sync_count != mfc->signal_count) {
        model_function_channel_free_flat_sync(mfc);
        mfc->scalar_sync.simbus_indices =
            calloc(mfc->signal_count, sizeof(uint32_t));
        mfc->scalar_sync.shadow = calloc(mfc->signal_count, sizeof(double));
        mfc->scalar_sync.output_shadow =
            calloc(mfc->signal_count, sizeof(double));
    }

    if (mfc->scalar_sync.simbus_indices == NULL ||
        mfc->scalar_sync.shadow == NULL ||
        mfc->scalar_sync.output_shadow == NULL) {
        model_function_channel_free_flat_sync(mfc);
        return;
    }

    for (uint32_t si = 0; si < mfc->signal_count; si++) {
        mfc->scalar_sync.simbus_indices[si] = sm[si].signal->vector_index;
    }
    mfc->scalar_sync.sync_count = mfc->signal_count;
    mfc->scalar_sync.enabled = true;
}

DLL_PRIVATE void model_function_channel_free_flat_sync(
    ModelFunctionChannel* mfc)
{
    if (mfc == NULL) return;
    if (mfc->scalar_sync.simbus_indices) {
        free(mfc->scalar_sync.simbus_indices);
        mfc->scalar_sync.simbus_indices = NULL;
    }
    if (mfc->scalar_sync.shadow) {
        free(mfc->scalar_sync.shadow);
        mfc->scalar_sync.shadow = NULL;
    }
    if (mfc->scalar_sync.output_shadow) {
        free(mfc->scalar_sync.output_shadow);
        mfc->scalar_sync.output_shadow = NULL;
    }
    mfc->scalar_sync.sync_count = 0;
    mfc->scalar_sync.simbus_scalar = NULL;
    mfc->scalar_sync.output_shadow_initialized = false;
    mfc->scalar_sync.enabled = false;
}

static void _legacy_transform_to_model(ModelFunctionChannel* mfc, SignalMap* sm)
{
    for (uint32_t si = 0; si < mfc->signal_count; si++) {
        mfc->signal_value_double[si] = sm[si].signal->val;
        mfc->signal_value_double_shadow[si] = sm[si].signal->val;
    }
}

static void _legacy_transform_from_model(
    ModelFunctionChannel* mfc, SignalMap* sm)
{
    for (uint32_t si = 0; si < mfc->signal_count; si++) {
        sm[si].signal->final_val = mfc->signal_value_double[si];
    }
}

DLL_PRIVATE void controller_transform_to_model(
    ModelFunctionChannel* mfc, SignalMap* sm, lua_State* L)
{
    /* Exactly two paths: legacy loop or flat_sync path. */
    if (mfc->signal_transform == NULL) {
        if (mfc->scalar_sync.enabled) {
            if (mfc->scalar_sync.sync_count != mfc->signal_count) {
                model_function_channel_build_flat_sync(mfc, sm);
            }
            return;
        }
        _legacy_transform_to_model(mfc, sm);
        return;
    }
    /* Transform path: */
    for (uint32_t si = 0; si < mfc->signal_count; si++) {
        double val = sm[si].signal->val;

        /* Linear transform: value * factor + offset */
        if (mfc->signal_transform[si].linear.factor != 0) {
            val = val * mfc->signal_transform[si].linear.factor +
                  mfc->signal_transform[si].linear.offset;
        }

        /* Functions: */
        int lua_ref = mfc->signal_transform[si].function.model.ref;
        if (lua_ref > 0) {
            val = _call_lua_transform(L, lua_ref, val);
        }

        /* Timing (phase/intercal effects). */
        // TODO: Lua delay library.

        /* Signal value. */
        mfc->signal_value_double[si] = val;
    }
}


DLL_PRIVATE void controller_transform_from_model(
    ModelFunctionChannel* mfc, SignalMap* sm, lua_State* L)
{
    /* Exactly two paths: legacy loop or flat_sync path. */
    if (mfc->signal_transform == NULL) {
        if (mfc->scalar_sync.enabled) {
            if (mfc->scalar_sync.sync_count != mfc->signal_count) {
                model_function_channel_build_flat_sync(mfc, sm);
            }
            return;
        }
        _legacy_transform_from_model(mfc, sm);
        return;
    }
    /* Transform path:*/
    for (uint32_t si = 0; si < mfc->signal_count; si++) {
        double val = mfc->signal_value_double[si];

        /* Linear transform: value * factor + offset */
        if (mfc->signal_transform[si].linear.factor != 0) {
            val = (mfc->signal_value_double[si] -
                      mfc->signal_transform[si].linear.offset) /
                  mfc->signal_transform[si].linear.factor;
        }

        /* Functions: */
        int lua_ref = mfc->signal_transform[si].function.vector.ref;
        if (lua_ref > 0) {
            val = _call_lua_transform(L, lua_ref, val);
        }

        /* Timing (phase/intercal effects). */
        // TODO: Lua delay library.

        /* Final value. */
        sm[si].signal->final_val = val;
    }
}
