// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
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


DLL_PRIVATE void controller_transform_to_model(
    ModelFunctionChannel* mfc, SignalMap* sm, lua_State* L)
{
    if (mfc->signal_transform) {
        for (uint32_t si = 0; si < mfc->signal_count; si++) {
            /* Linear transform: value * factor + offset */
            if (mfc->signal_transform[si].linear.factor != 0) {
                mfc->signal_value_double[si] =
                    sm[si].signal->val *
                        mfc->signal_transform[si].linear.factor +
                    mfc->signal_transform[si].linear.offset;
            } else {
                // Disabled (ie. div 0), direct.
                mfc->signal_value_double[si] = sm[si].signal->val;
            }

            /* Functions: */
            mfc->signal_value_double[si] = _call_lua_transform(L,
                mfc->signal_transform[si].function.model.ref,
                mfc->signal_value_double[si]);

            /* Timing (phase/intercal effects). */
            // TODO: Lua delay library.
        }
    } else {
        for (uint32_t si = 0; si < mfc->signal_count; si++) {
            mfc->signal_value_double[si] = sm[si].signal->val;
        }
    }
}


DLL_PRIVATE void controller_transform_from_model(
    ModelFunctionChannel* mfc, SignalMap* sm, lua_State* L)
{
    if (mfc->signal_transform) {
        for (uint32_t si = 0; si < mfc->signal_count; si++) {
            /* Linear transform: value * factor + offset */
            if (mfc->signal_transform[si].linear.factor != 0) {
                sm[si].signal->final_val =
                    (mfc->signal_value_double[si] -
                        mfc->signal_transform[si].linear.offset) /
                    mfc->signal_transform[si].linear.factor;
            } else {
                // Disabled, direct.
                sm[si].signal->final_val = mfc->signal_value_double[si];
            }

            /* Functions: */
            sm[si].signal->final_val = _call_lua_transform(L,
                mfc->signal_transform[si].function.vector.ref,
                sm[si].signal->final_val);

            /* Timing (phase/intercal effects). */
            // TODO: Lua delay library.
        }
    } else {
        for (uint32_t si = 0; si < mfc->signal_count; si++) {
            sm[si].signal->final_val = mfc->signal_value_double[si];
        }
    }
}
