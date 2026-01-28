// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <dse/clib/util/yaml.h>
#include <dse/clib/util/cleanup.h>
#include <dse/modelc/mcl.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/model/lua.h>
#include <dse/platform.h>
#include <dse/logger.h>


#define UNUSED(x)               ((void)x)

#define LUA_MODEL_CREATE        "model_create"
#define LUA_MODEL_STEP          "model_step"
#define LUA_MODEL_DESTROY       "model_destroy"
#define LUA_FILE_EXTENSION      ".lua"

#define LUA_MI_RUNTIME_MCL      "runtime/mcl"
#define LUA_MI_RUNTIME_MCL_NAME "lua"
#define LUA_MI_RUNTIME_FILES    "runtime/files"


/**
Lua Model API
=============
*/

lua_State* lua_model_create(ModelDesc* m)
{
    lua_State* L;
    L = luaL_newstate();
    luaL_openlibs(L);
    if (m != NULL) {
        lua_modellib_open(L, m);
    }
    return L;
}

void lua_model_destroy(lua_State* L)
{
    lua_close(L);
}

static void lua_model_error(lua_State* L, const char* msg)
{
    log_error("Lua Error: %s (%s)", msg, lua_tostring(L, -1));
}

static void lua_model_fatal(lua_State* L, const char* msg)
{
    log_fatal("Lua Error: %s (%s)", msg, lua_tostring(L, -1));
}


int lua_model_pcall(lua_State* L, const char* func, int* result)
{
    assert(L);

    lua_getglobal(L, func);
    if (!lua_isfunction(L, -1)) {
        log_trace("Function not available: %s", func);
        return EINVAL;
    }
    if (lua_pcall(L, 0, 1, 0)) {
        lua_model_error(L, "lua_pcall() failed");
        return EBADF;
    }
    if (result) {
        *result = lua_tonumber(L, -1);
        log_trace("Function %s() returned %d", func, *result);
    }

    return 0;
}


/**
Lua MCL
=======
*/

static int32_t lua_mcl_load(MclDesc* mcl)
{
    assert(mcl);
    LuaMclModel* m = (LuaMclModel*)mcl;
    assert(m->mcl.model.mi);
    assert(m->mcl.model.mi->private);
    ModelInstancePrivate* mip = m->mcl.model.mi->private;

    /* Establish the Lua interpreter. */
    if (mip->lua_state == NULL) {
        mip->lua_state = lua_model_create(&m->mcl.model);
        assert(mip->lua_state);
    }
    m->L = mip->lua_state;

    /* Parse runtime parameters. */
    assert(m->mcl.model.mi->spec);

    const char* mcl_name =
        dse_yaml_get_scalar(m->mcl.model.mi->spec, LUA_MI_RUNTIME_MCL);
    if (mcl_name == NULL || strcmp(mcl_name, LUA_MI_RUNTIME_MCL_NAME)) {
        log_error(
            "Lua MCL bad config in Model Instance (path:" LUA_MI_RUNTIME_MCL
            " , value:%s)",
            mcl_name);
        return EINVAL;
    }

    size_t files_len = 0;
    CLEANUP_P(str_arr, files) = (str_arr)dse_yaml_get_array(
        m->mcl.model.mi->spec, LUA_MI_RUNTIME_FILES, &files_len);
    // Locate the first ".lua" file.
    for (size_t i = 0; i < files_len; i++) {
        if (strlen(files[i]) < strlen(LUA_FILE_EXTENSION)) continue;
        if (strncmp(files[i] + (strlen(files[i]) - strlen(LUA_FILE_EXTENSION)),
                LUA_FILE_EXTENSION, strlen(LUA_FILE_EXTENSION)) == 0) {
            m->lua_model_path = files[i];
            break;
        }
    }
    if (m->lua_model_path == NULL) {
        log_error("Lua model file not specified in Model Instance "
                  "(path: " LUA_MI_RUNTIME_FILES ")");
        return EINVAL;
    }

    return 0;
}

static int32_t lua_mcl_init(MclDesc* mcl)
{
    assert(mcl);
    LuaMclModel* m = (LuaMclModel*)mcl;

    /* Load and configure the provided Lua Model. */
    log_notice("Lua MCL: loading model: %s", m->lua_model_path);
    if (luaL_loadfile(m->L, m->lua_model_path) != 0) {
        lua_model_error(m->L, "luaL_loadfile() failed");
        return EINVAL;
    }
    if (lua_pcall(m->L, 0, 0, 0)) {
        lua_model_fatal(m->L, "Model global init: lua_pcall() failed");
        return EINVAL;
    }

    /* Call the Lua model. */
    int result = 0;
    int rc = lua_model_pcall(m->L, LUA_MODEL_CREATE, &result);
    log_trace("Create: lua_model_pcall() rc=%d, result=%d", rc, result);
    switch (rc) {
    case EBADF:
        return EBADF;
    case EINVAL:
        // No create function in the model, not an error condition.
        break;
    case 0:
        if (result) {
            log_error(LUA_MODEL_CREATE "() returned %d", result);
            return result;
        }
        break;
    default:
        break;
    }

    return 0;
}

static int32_t lua_mcl_step(MclDesc* mcl, double* model_time, double end_time)
{
    assert(mcl);
    LuaMclModel* m = (LuaMclModel*)mcl;
    assert(model_time);
    log_trace("Step: model_time: %f, end_time: %f", *model_time, end_time);

    /* Call the Lua model. */
    int result = 0;
    int rc = lua_model_pcall(m->L, LUA_MODEL_STEP, &result);
    log_trace("Step: lua_model_pcall() rc=%d, result=%d", rc, result);
    switch (rc) {
    case EBADF:
        return EBADF;
    case EINVAL:
        log_error(LUA_MODEL_STEP "() returned %d", result);
        return EINVAL;
    case 0:
        if (result) {
            log_error(LUA_MODEL_STEP "() returned %d", result);
            return result;
        }
        break;
    default:
        break;
    }

    *model_time = end_time;
    return 0;
}

static int32_t lua_mcl_marshal_in(MclDesc* mcl)
{
    UNUSED(mcl);
    return 0;
}

static int32_t lua_mcl_marshal_out(MclDesc* mcl)
{
    UNUSED(mcl);
    return 0;
}

static int32_t lua_mcl_unload(MclDesc* mcl)
{
    assert(mcl);
    LuaMclModel* m = (LuaMclModel*)mcl;

    /* Call the Lua model. */
    int result = 0;
    int rc = lua_model_pcall(m->L, LUA_MODEL_DESTROY, &result);
    log_trace("Destroy: lua_model_pcall() rc=%d, result=%d", rc, result);
    switch (rc) {
    case EBADF:
        return EBADF;
    case EINVAL:
        // No destroy function in the model, not an error condition.
        break;
    case 0:
        if (result) {
            log_error(LUA_MODEL_DESTROY "() returned %d", result);
            return result;
        }
        break;
    default:
        break;
    }

    lua_model_destroy(m->L);
    m->L = NULL;

    /* m->L shadows mip->lua_state, so clear that too. */
    assert(m->mcl.model.mi);
    assert(m->mcl.model.mi->private);
    ModelInstancePrivate* mip = m->mcl.model.mi->private;
    mip->lua_state = NULL;

    return 0;
}


MclDesc* lua_mcl_create(ModelDesc* model)
{
    LuaMclModel* m = calloc(1, sizeof(LuaMclModel));
    memcpy(m, model, sizeof(ModelDesc));

    m->mcl.vtable = (struct MclVTable){
        .load = lua_mcl_load,
        .init = lua_mcl_init,
        .step = lua_mcl_step,
        .marshal_out = lua_mcl_marshal_out,
        .marshal_in = lua_mcl_marshal_in,
        .unload = lua_mcl_unload,
    };

    return (MclDesc*)m;
}

void lua_mcl_destroy(MclDesc* model)
{
    LuaMclModel* m = (LuaMclModel*)model;
    UNUSED(m);
}
