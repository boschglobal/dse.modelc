// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/model/lua.h>
#include <dse/platform.h>
#include <dse/logger.h>


#define LUA_MODELLIB_NAME       "model"
#define LUA_MODELLIB_UNIQUE_KEY "__library_id"
#define LUA_MODELLIB_UNIQUE_ID  "dse:modelc:luamodel"
#define LUA_MODELLIB_SV_TNAME   "sv"
#define LUA_NL                  luaL_addstring(&b, "\n")


/**
Lua Model Library
=================
*/

static int lua_modellib__version(lua_State* L)
{
    lua_pushstring(L, MODELC_VERSION);
    return 1;
}

static int _format_lua_log(lua_State* L)
{
    // clang-format off
    // Stack on entry: [table][fmt][args...]
    int nargs = lua_gettop(L) - 1;
    if (nargs < 1) {
        return EINVAL;
    }

    /* Format the string with Lua. */
    lua_getglobal(L, "string");                             // [lib]
    if (!lua_istable(L, -1)) {
        luaL_error(L, "string library not available");
        return EINVAL;
    }
    lua_getfield(L, -1, "format");                          // [lib][func]
    if (!lua_isfunction(L, -1)) {
        luaL_error(L, "string.format not available");
        return EINVAL;
    }
    lua_remove(L, -2);                                      // [func]
    for (int i = 1; i <= nargs; i++) {
        /* Offset for the [table] ATOS. */
        lua_pushvalue(L, i+1);                              // [func][fmt][N args..]
    }

    /* Return the string on the stack. */
    lua_call(L, nargs, 1);                                  // [args...][str]
    lua_replace(L, 1);
    lua_settop(L, 1);                                       // [str]
    // Stack on exit: [str]
    // clang-format on
    return 0;
}

static int lua_modellib__log_error(lua_State* L)
{
    int rc = _format_lua_log(L);
    if (rc != 0) return 0; /* NOP. */

    // clang-format off
    const char* log = lua_tostring(L, -1);                  // [str]
    log_error(log);
    lua_pop(L, 1);                                          // (empty stack)
    // clang-format on
    return 0;
}

static int lua_modellib__log_notice(lua_State* L)
{
    int rc = _format_lua_log(L);
    if (rc != 0) return 0; /* NOP. */

    // clang-format off
    const char* log = lua_tostring(L, -1);                  // [str]
    log_notice(log);
    lua_pop(L, 1);                                          // (empty stack)
    // clang-format on
    return 0;
}

static int lua_modellib__log_info(lua_State* L)
{
    int rc = _format_lua_log(L);
    if (rc != 0) return 0; /* NOP. */

    // clang-format off
    const char* log = lua_tostring(L, -1);                  // [str]
    log_info(log);
    lua_pop(L, 1);                                          // (empty stack)
    // clang-format on
    return 0;
}

static int lua_modellib__log_debug(lua_State* L)
{
    int rc = _format_lua_log(L);
    if (rc != 0) return 0; /* NOP. */

    // clang-format off
    const char* log = lua_tostring(L, -1);                  // [str]
    log_debug(log);
    lua_pop(L, 1);                                          // (empty stack)
    // clang-format on
    return 0;
}

static int lua_modellib__model_time(lua_State* L)
{
    ModelDesc* m = (ModelDesc*)lua_touserdata(L, lua_upvalueindex(1));
    assert(m);
    MclDesc* mcl = (MclDesc*)m;
    lua_pushnumber(L, mcl->model_time);
    return 1;
}

static int lua_modellib__step_size(lua_State* L)
{
    ModelDesc* m = (ModelDesc*)lua_touserdata(L, lua_upvalueindex(1));
    assert(m);
    MclDesc* mcl = (MclDesc*)m;
    if (mcl->step_size) {
        lua_pushnumber(L, mcl->step_size);
        return 1;
    }
    assert(m->sim);
    lua_pushnumber(L, m->sim->step_size);
    return 1;
}

static int lua_modellib__tostring(lua_State* L)
{
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addstring(&b, "DSE ModelC - LUA MODEL LIB");
    LUA_NL;
    luaL_addstring(&b, "[model library: " LUA_MODELLIB_UNIQUE_ID "]");
    LUA_NL;
    luaL_addstring(&b, "[version: " MODELC_VERSION "]");
    LUA_NL;
    luaL_addstring(&b, "[platform: " PLATFORM_OS "-" PLATFORM_ARCH "]");
    luaL_pushresult(&b);
    return 1;
}

static const luaL_Reg lua_modellib_meta_methods[] = {
    { "version", lua_modellib__version },
    { "log_error", lua_modellib__log_error },
    { "log_notice", lua_modellib__log_notice },
    { "log_info", lua_modellib__log_info },
    { "log_debug", lua_modellib__log_debug },
    { "step_size", lua_modellib__step_size },
    { "model_time", lua_modellib__model_time },
    { NULL, NULL },
};


/*
LUA: SV Instance
================
*/
static int luasv_signal__index(lua_State* L)
{
    SignalVector* sv = (SignalVector*)lua_touserdata(L, lua_upvalueindex(1));

    if (lua_type(L, 2) == LUA_TNUMBER) {
        int idx = lua_tointeger(L, 2) - 1; /* 0-based index. */
        if (sv && (idx >= 0 && idx < (int)sv->count)) {
            lua_pushstring(L, sv->signal[idx]);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int luasv_scalar__index(lua_State* L)
{
    ModelDesc*    m = (ModelDesc*)lua_touserdata(L, lua_upvalueindex(1));
    SignalVector* sv = (SignalVector*)lua_touserdata(L, lua_upvalueindex(2));

    if (lua_type(L, 2) == LUA_TNUMBER) {
        int idx = lua_tointeger(L, 2) - 1; /* 0-based index. */
        if (sv && (idx >= 0 && idx < (int)sv->count)) {
            lua_pushnumber(L, sv->scalar[idx]);
            return 1;
        }
    } else if (lua_type(L, 2) == LUA_TSTRING) {
        const char* name = lua_tostring(L, 2);
        if (name == NULL) {
            lua_pushnil(L);
            return 1;
        }
        ModelSignalIndex idx = signal_index(m, sv->alias, name);
        if (idx.scalar) {
            lua_pushnumber(L, *idx.scalar);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

static int luasv_scalar__newindex(lua_State* L)
{
    ModelDesc*    m = (ModelDesc*)lua_touserdata(L, lua_upvalueindex(1));
    SignalVector* sv = (SignalVector*)lua_touserdata(L, lua_upvalueindex(2));

    if (lua_type(L, 3) != LUA_TNUMBER) {
        return 0;
    }
    double value = lua_tonumber(L, 3);
    if (lua_type(L, 2) == LUA_TNUMBER) {
        int idx = lua_tointeger(L, 2) - 1; /* 0-based index. */
        if (sv && (idx >= 0 && idx < (int)sv->count)) {
            sv->scalar[idx] = value;
            return 0;
        }
    } else if (lua_type(L, 2) == LUA_TSTRING) {
        const char* name = lua_tostring(L, 2);
        if (name == NULL) {
            return 0;
        }
        ModelSignalIndex idx = signal_index(m, sv->alias, name);
        if (idx.scalar) {
            *idx.scalar = value;
            return 0;
        }
    }

    return 0;
}

static int luasv_sv__len(lua_State* L)
{
    SignalVector* sv = (SignalVector*)lua_touserdata(L, lua_upvalueindex(1));
    lua_pushinteger(L, sv->count);
    return 1;
}

static int luasv_instance__tostring(lua_State* L)
{
    // clang-format off
    // Stack on entry: [instance]
    lua_getfield(L, 1, "alias");                            // [instance][alias]
    const char* alias = lua_tostring(L, -1);
    lua_pop(L, 1);                                          // [instance]

    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addstring(&b, "[signal vector: ");
    luaL_addstring(&b, alias ? alias : "(no alias)");
    luaL_addstring(&b, " ]");
    luaL_pushresult(&b);                                    // [instance][string]

    return 1;
    // clang-format on
}

static int luasv_instance_find(lua_State* L)
{
    // clang-format off
    // Stack on entry: [instance][name]
    ModelDesc* m = (ModelDesc*)lua_touserdata(L, lua_upvalueindex(1));

    if (lua_type(L, 2) == LUA_TSTRING) {
        const char* name = lua_tostring(L, 2);              // [instance][name]
        lua_getfield(L, 1, "alias");                        // [instance][name][alias]
        const char* alias = lua_tostring(L, -1);
        if (alias == NULL || name == NULL) {
            lua_pushnil(L);                                 // [instance][nil]
            return 1;
        }
        ModelSignalIndex idx = signal_index(m, alias, name);
        if (idx.scalar) {
            /* 1-based index. */
            lua_pushinteger(L, idx.signal + 1);             // [instance][idx]
            return 1;
        }
    }

    lua_pushnil(L);                                         // [instance][nil]
    return 1;
    // clang-format on
}

static int luasv_instance_group_annotation(lua_State* L)
{
    // clang-format off
    // Stack on entry: [instance][name]
    SignalVector* sv = (SignalVector*)lua_touserdata(L, lua_upvalueindex(2));

    if (lua_type(L, 2) == LUA_TSTRING) {
        const char* name = lua_tostring(L, 2);              // [instance][name]
        lua_pop(L, 1);                                      // [instance]
        if (name == NULL) {
            lua_pushnil(L);                                 // [instance][nil]
            return 1;
        }
        const char* value;
        value = signal_group_annotation(sv, name, NULL);
        if (value != NULL) {
            lua_pushstring(L, value);                       // [instance][value]
            return 1;
        }
    }

    lua_pushnil(L);                                         // [instance][nil]
    return 1;
    // clang-format on
}

static int luasv_instance_signal_annotation(lua_State* L)
{
    // clang-format off
    // Stack on entry: [instance][name][annotation]
    ModelDesc* m = (ModelDesc*)lua_touserdata(L, lua_upvalueindex(1));
    SignalVector* sv = (SignalVector*)lua_touserdata(L, lua_upvalueindex(2));
    const char*   name = NULL;
    const char*   annotation = NULL;
    uint32_t      sig_idx = 0;

    if (lua_type(L, 2) == LUA_TSTRING) {
        name = lua_tostring(L, 2);
    }
    if (lua_type(L, 3) == LUA_TSTRING) {
        annotation = lua_tostring(L, 3);
    }
    if (name == NULL || annotation == NULL) {
        lua_pushnil(L);                                     // [instance][nil]
        return 1;
    }

    ModelSignalIndex idx = signal_index(m, sv->alias, name);
    if (idx.scalar == NULL) {
        lua_pushnil(L);
        return 1;
    }
    sig_idx = idx.vector;

    const char* value;
    value = signal_annotation(sv, sig_idx, annotation, NULL);
    if (value != NULL) {
        lua_pushstring(L, value);                           // [instance][value]
        return 1;
    }

    lua_pushnil(L);                                         // [instance][nil]
    return 1;
    // clang-format on
}

static const luaL_Reg luasv_instance_methods[] = {
    { "find", luasv_instance_find },
    { "group_annotation", luasv_instance_group_annotation },
    { "annotation", luasv_instance_signal_annotation },
    { NULL, NULL },
};

static void luasv_push_registry_key(lua_State* L, int idx)
{
    char key[32];
    snprintf(key, sizeof(key), "model:sv::%d", idx), lua_pushstring(L, key);
}

static void luasv_push_registry_key_by_alias(lua_State* L, const char* alias)
{
    char key[64];
    snprintf(key, sizeof(key), "model:sv:%s", alias), lua_pushstring(L, key);
}


static void luasv_register_instance(
    lua_State* L, int idx, SignalVector* sv, ModelDesc* m)
{
    assert(m);
    assert(sv);
    assert(sv->alias);

    // clang-format off
    /* Create instance. */
    lua_newtable(L);                                        // [instance]
    lua_pushstring(L, sv->alias);                           // [instance][alias]
    lua_setfield(L, -2, "alias");                           // [instance]

    lua_newtable(L);                                        // [instance][metatable]
    lua_pushlightuserdata(L, m);                            // [instance][metatable][upvalue]
    lua_pushlightuserdata(L, sv);                           // [instance][metatable][upvalue][upvalue] NOLINT

    luaL_setfuncs(L, luasv_instance_methods, 2);            // [instance][metatable]
    lua_pushvalue(L, -1);                                   // [instance][metatable][metatable]
    lua_setfield(L, -2, "__index");                         // [instance][metatable]
    lua_pushcfunction(L, luasv_instance__tostring);         // [instance][metatable][__tostring]
    lua_setfield(L, -2, "__tostring");                      // [instance][metatable]
    lua_setmetatable(L, -2);                                // [instance]

    /* Setup registry indexes. */
    luasv_push_registry_key(L, idx);                        // [instance][key]
    lua_pushvalue(L, -2);                                   // [instance][key][instance]
    lua_settable(L, LUA_REGISTRYINDEX);                     // [instance]

    luasv_push_registry_key_by_alias(L, sv->alias);         // [instance][alias_key]
    lua_pushvalue(L, -2);                                   // [instance][alias_key][instance]
    lua_settable(L, LUA_REGISTRYINDEX);                     // [instance]

    /* Proxy metatable for signal array. */
    lua_newtable(L);                                        // [instance][signal]
    lua_newtable(L);                                        // [instance][signal][metatable]
    lua_pushlightuserdata(L, sv);                           // [instance][signal][metatable][upvalue] NOLINT
    lua_pushcclosure(L, luasv_signal__index, 1);            // [instance][signal][metatable][__index] NOLINT
    lua_setfield(L, -2, "__index");                         // [instance][signal][metatable]
    lua_pushlightuserdata(L, sv);                           // [instance][signal][metatable][upvalue] NOLINT
    lua_pushcclosure(L, luasv_sv__len, 1);                  // [instance][signal][metatable][__len]
    lua_setfield(L, -2, "__len");                           // [instance][signal][metatable]
    lua_setmetatable(L, -2);                                // [instance][signal]
    lua_setfield(L, -2, "signal");                          // [instance]

    /* Proxy metatable for scalar array. */
    lua_newtable(L);                                        // [instance][scalar]
    lua_newtable(L);                                        // [instance][scalar][metatable]
    lua_pushlightuserdata(L, m);                            // [instance][scalar][metatable][upvalue] NOLINT
    lua_pushlightuserdata(L, sv);                           // [instance][scalar][metatable][upvalue][upvalue] NOLINT
    lua_pushcclosure(L, luasv_scalar__index, 2);            // [instance][scalar][metatable][__index] NOLINT
    lua_setfield(L, -2, "__index");                         // [instance][scalar][metatable]
    lua_pushlightuserdata(L, m);                            // [instance][scalar][metatable][upvalue] NOLINT
    lua_pushlightuserdata(L, sv);                           // [instance][scalar][metatable][upvalue][upvalue] NOLINT
    lua_pushcclosure(L, luasv_scalar__newindex, 2);         // [instance][scalar][metatable][__newindex] NOLINT
    lua_setfield(L, -2, "__newindex");                      // [instance][scalar][metatable]
    lua_pushlightuserdata(L, sv);                           // [instance][scalar][metatable][upvalue] NOLINT
    lua_pushcclosure(L, luasv_sv__len, 1);                  // [instance][scalar][metatable][__len]
    lua_setfield(L, -2, "__len");                           // [instance][scalar][metatable]
    lua_setmetatable(L, -2);                                // [instance][scalar]
    lua_setfield(L, -2, "scalar");                          // [instance]

    lua_pop(L, 1);  // (stack at entry)
    // clang-format on
}


/*
LUA: MODEL.SV
=============
*/
static int luasv__index(lua_State* L)
{
    if (lua_type(L, 2) == LUA_TNUMBER) {
        int idx = lua_tointeger(L, 2);
        luasv_push_registry_key(L, idx);
        lua_gettable(L, LUA_REGISTRYINDEX);
        return 1;
    } else if (lua_type(L, 2) == LUA_TSTRING) {
        const char* alias = lua_tostring(L, 2);
        luasv_push_registry_key_by_alias(L, alias);
        lua_gettable(L, LUA_REGISTRYINDEX);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

static int luasv__len(lua_State* L)
{
    SignalVector* sv = (SignalVector*)lua_touserdata(L, lua_upvalueindex(1));
    int           count = 0;

    for (SignalVector* _sv = sv; _sv && _sv->alias; _sv++) {
        count++;
    }

    lua_pushinteger(L, count);
    return 1;
}

static int luasv__tostring(lua_State* L)
{
    SignalVector* sv = (SignalVector*)lua_touserdata(L, lua_upvalueindex(1));
    luaL_Buffer   b;
    luaL_buffinit(L, &b);
    luaL_addstring(&b, "[signal vectors] (");
    for (SignalVector* _sv = sv; _sv && _sv->alias; _sv++) {
        luaL_addstring(&b, " ");
        luaL_addstring(&b, sv->alias);
    }
    luaL_addstring(&b, " )");
    luaL_pushresult(&b);
    return 1;
}

static const luaL_Reg luasv_methods[] = {
    { NULL, NULL },
};


/*
LUA: Model Library
==================
*/
int lua_modellib_open(lua_State* L, ModelDesc* m)
{
    assert(L);
    assert(m);

    // Check if 'model' is already open.
    lua_getglobal(L, LUA_MODELLIB_NAME);
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, LUA_MODELLIB_UNIQUE_KEY);
        if (lua_isstring(L, -1) &&
            strcmp(lua_tostring(L, -1), LUA_MODELLIB_UNIQUE_ID) == 0) {
            /* Lib 'model' is installed. */
            lua_pop(L, 2);
            return 0;
        } else {
            lua_pop(L, 2);
            log_fatal("Failed to open Lua Model library, global conflict");
            return 1;
        }
    } else {
        /* Lib 'model' not installed, continue. */
        lua_pop(L, 1);
    }

    /* Lua Model Library. */
    // clang-format off
    /* Create the 'model' lib. */
    lua_newtable(L);                                        // [model]
    lua_pushstring(L, LUA_MODELLIB_UNIQUE_ID);              // [model][string]
    lua_setfield(L, -2, LUA_MODELLIB_UNIQUE_KEY);           // [model]
    lua_newtable(L);                                        // [model][metatable]
    lua_pushlightuserdata(L, m);                            // [model][metatable][upvalue]
    luaL_setfuncs(L, lua_modellib_meta_methods, 1);         // [model][metatable]
    lua_pushcfunction(L, lua_modellib__tostring);           // [model][metatable][__tostring]
    lua_setfield(L, -2, "__tostring");                      // [model][metatable]
    lua_pushvalue(L, -1);                                   // [model][metatable][metatable]
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);                                // [model]

    /* Create signal vector tables. */
    lua_newtable(L);                                        // [model][sv]
    lua_newtable(L);                                        // [model][sv][metatable]
    luaL_setfuncs(L, luasv_methods, 0);                     // [model][sv][metatable]
    lua_pushcfunction(L, luasv__index);                     // [model][sv][metatable][__index]
    lua_setfield(L, -2, "__index");                         // [model][sv][metatable]
    lua_pushlightuserdata(L, m->sv);                        // [model][sv][metatable][upvalue]
    lua_pushcclosure(L, luasv__len, 1);                     // [model][sv][metatable][__len]
    lua_setfield(L, -2, "__len");                           // [model][sv][metatable]
    lua_pushlightuserdata(L, m->sv);                        // [model][sv][metatable][upvalue]
    lua_pushcclosure(L, luasv__tostring, 1);                // [model][sv][metatable][__tostring]
    lua_setfield(L, -2, "__tostring");                      // [model][sv][metatable]
    lua_setmetatable(L, -2);                                // [model][sv]

    int idx = 0;
    for (SignalVector* sv = m->sv; sv && sv->alias; sv++) {
        luasv_register_instance(L, ++idx, sv, m);           // [model][sv]
    }

    /* Install the model. */
    lua_setfield(L, -2, LUA_MODELLIB_SV_TNAME);                  // [model]
    lua_setglobal(L, LUA_MODELLIB_NAME);                    // (empty)
    // clang-format on

    return 0;
}
