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
#include <dse/clib/util/strings.h>
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

static void lua_model_error(lua_State* L, const char* msg)
{
    log_error("Lua Error: %s (%s)", msg, lua_tostring(L, -1));
}

static void lua_model_fatal(lua_State* L, const char* msg)
{
    log_fatal("Lua Error: %s (%s)", msg, lua_tostring(L, -1));
}


lua_State* lua_model_create(lua_State* L, ModelDesc* m)
{
    if (L == NULL) {
        L = luaL_newstate();
        luaL_openlibs(L);

#ifdef _WIN32
        lua_getglobal(L, "package");
        if (lua_istable(L, -1)) {
            const char* path = NULL;
            lua_getfield(L, -1, "path");
            path = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_pushfstring(L,
                "./share/lua/5.4/?.lua;./share/lua/5.4/?/init.lua;%s",
                path ? path : "");
            lua_setfield(L, -2, "path");
        }
        lua_pop(L, 1);
#endif
    } else {
        luaL_openlibs(L);
    }

    if (m != NULL) {
        lua_modellib_open(L, m);
    }
    return L;
}

void lua_model_destroy(lua_State* L)
{
    lua_close(L);
}

int lua_install_script(lua_State* L, const char* lua_script)
{
    if (lua_script == NULL) return 0;
    assert(L);

    /* Run the Lua script. */
    int top = lua_gettop(L);
    if (lua_script != NULL && luaL_dostring(L, lua_script) != 0) {
        log_error(
            "Lua Error: luaL_dostring() failed (%s)", lua_tostring(L, -1));
        lua_settop(L, top);
        return 0;
    }

    /* If the script returned a function, store and return a reference. */
    if (lua_isfunction(L, -1)) {
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        if (ref < 0) {
            log_error("Failed to create reference for returned Lua function");
            lua_settop(L, top);
            return 0;
        }
        log_notice(
            "Lua: anonymous function installed: (ref=%d)\n%s", ref, lua_script);
        lua_settop(L, top);
        return ref;
    }

    lua_settop(L, top);
    log_notice("Lua: script installed: \n%s", lua_script);
    return 0;
}

int lua_push_ctx(lua_State* L)
{
    // clang-format off
    lua_newtable(L);                                        // [ctx]
    return 0;
    // clang-format on
}

int lua_ctx_set_double(lua_State* L, const char* name, double value)
{
    // clang-format off
    if (lua_gettop(L) < 1 || !lua_istable(L, -1)) {         // [ctx]
        lua_newtable(L);                                    // [ctx]
    }
    lua_pushnumber(L, value);                               // [ctx][value]
    lua_setfield(L, -2, name);                              // [ctx]
    return 0;
    // clang-format on
}

int lua_ctx_get_double(lua_State* L, const char* name, double* value)
{
    // clang-format off
    if (value == NULL) {
        return -EINVAL;
    }
    if (lua_gettop(L) < 1 || !lua_istable(L, -1)) {
        return -EINVAL;
    }

    lua_getfield(L, -1, name);                              // [ctx][value]
    if (lua_isnumber(L, -1)) {
        *value = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);                                          // [ctx]
    return 0;
    // clang-format on
}

int lua_call_ctx(lua_State* L, int32_t func_ref)
{
    // clang-format off
    /* A ctx table should already be pushed on the stack. */
    if (lua_gettop(L) < 1 || !lua_istable(L, -1)) {         // [ctx]
        return -EINVAL;
    }

    /* Call function(ctx). */
    lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);            // [ctx][func]
    if (!lua_isfunction(L, -1)) {                           // [ctx][val]
        lua_pop(L, 2);                                      // []
        return -EINVAL;
    }
    lua_insert(L, -2);                                      // [func][ctx]
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {                  // [ret|err]
        lua_model_error(L, "lua_pcall() failed");
        lua_pop(L, 1);                                      // []
        return -1;
    }

    /* If the function did not return a table, return now. */
    if (!lua_istable(L, -1)) {                              // [ret]
        lua_pop(L, 1);                                      // []
        return 0;
    }

    /* If the function returned a ctx table, check the err state. */
    int idx = lua_gettop(L);                                // [ctx]
    int err = 0;                                            // [ctx]
    lua_getfield(L, idx, "err");                            // [ctx][err]
    if (lua_isinteger(L, -1)) {                             // [ctx][err]
        err = (int)lua_tointeger(L, -1);                    // [ctx][err]
    }
    lua_pop(L, 1);                                          // [ctx]
    if (err) {
        lua_getfield(L, idx, "errmsg");                     // [ctx][errmsg]
        if (lua_isstring(L, -1)) {
            const char* msg = lua_tostring(L, -1);          // [ctx][errmsg]
            if (msg) {
                if (__log_level__ != LOG_QUIET)
                    log_error("lua call returned error: %s (%d)", msg, err);
            }
        }
        lua_pop(L, 2);                                      // []
        return -1;
    }

    /* Return with the ctx table still on the stack. */
    return 1;                                               // [ctx]
    // clang-format on
}

int lua_pop_ctx(lua_State* L)
{
    // clang-format off
    if (lua_gettop(L) > 0 && lua_istable(L, -1)) {          // [ctx]
        lua_pop(L, 1);                                      // []
    }
    return 0;
    // clang-format on
}


int lua_model_pcall(lua_State* L, const char* func, int* result)
{
    assert(L);
    int top = lua_gettop(L);

    lua_getglobal(L, func);
    if (!lua_isfunction(L, -1)) {
        log_trace("Function not available: %s", func);
        lua_settop(L, top);
        return EINVAL;
    }
    if (lua_pcall(L, 0, 1, 0)) {
        lua_model_error(L, "lua_pcall() failed");
        lua_settop(L, top);
        return EBADF;
    }
    if (result) {
        *result = (int)lua_tointeger(L, -1);
        log_trace("Function %s() returned %d", func, *result);
    }

    lua_settop(L, top);
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
    mip->lua_state = lua_model_create(mip->lua_state, &m->mcl.model);
    assert(mip->lua_state);
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
            m->lua_model_path =
                dse_path_cat(m->mcl.model.sim->sim_path, files[i]);
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

    free(m->lua_model_path);

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
