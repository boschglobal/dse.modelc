// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <dse/modelc/model/lua.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/model/pdunet/network.h>


typedef struct ctx_t {
    double   phys;
    uint64_t raw;
    uint8_t* payload;
    uint32_t payload_len;

    /* Lua function may return these table fields to indicate an error. */
    int         err;
    const char* errmsg;
} ctx_t;


/* Payload metatable. */

typedef struct {
    uint8_t* data;
    uint32_t len;
} payload_wrapper_t;

#define PAYLOAD_METATABLE "pdu_payload_array"

static int _payload_index(lua_State* L)
{
    payload_wrapper_t* pw =
        (payload_wrapper_t*)luaL_checkudata(L, 1, PAYLOAD_METATABLE);
    lua_Integer idx = luaL_checkinteger(L, 2);

    if (idx < 1 || idx > pw->len) {
        return luaL_error(
            L, "payload index %d out of bounds (1..%d)", idx, pw->len);
    }

    lua_pushinteger(L, pw->data[idx - 1]);
    return 1;
}

static int _payload_newindex(lua_State* L)
{
    payload_wrapper_t* pw =
        (payload_wrapper_t*)luaL_checkudata(L, 1, PAYLOAD_METATABLE);
    lua_Integer idx = luaL_checkinteger(L, 2);
    lua_Integer value = luaL_checkinteger(L, 3);

    if (idx < 1 || idx > pw->len) {
        return luaL_error(
            L, "payload index %d out of bounds (1..%d)", idx, pw->len);
    }
    if (value < 0 || value > 255) {
        return luaL_error(L, "payload value %d out of range (0..255)", value);
    }

    pw->data[idx - 1] = (uint8_t)value;
    return 0;
}

static int _payload_len(lua_State* L)
{
    payload_wrapper_t* pw =
        (payload_wrapper_t*)luaL_checkudata(L, 1, PAYLOAD_METATABLE);
    lua_pushinteger(L, pw->len);
    return 1;
}


static void _create_payload_metatable(lua_State* L)
{
    if (luaL_newmetatable(L, PAYLOAD_METATABLE)) {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _payload_index);
        lua_settable(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _payload_newindex);
        lua_settable(L, -3);

        lua_pushstring(L, "__len");
        lua_pushcfunction(L, _payload_len);
        lua_settable(L, -3);
    }
    lua_pop(L, 1);
}


static void _push_payload_userdata(lua_State* L, uint8_t* data, uint32_t len)
{
    payload_wrapper_t* pw =
        (payload_wrapper_t*)lua_newuserdata(L, sizeof(payload_wrapper_t));
    pw->data = data;
    pw->len = len;

    luaL_getmetatable(L, PAYLOAD_METATABLE);
    lua_setmetatable(L, -2);
}


static void _lua_push_pdu_ctx(
    lua_State* L, uint8_t* payload, uint32_t payload_len)
{
    // clang-format off
    lua_newtable(L);                                        // [table]
    lua_pushstring(L, "payload");                           // [table][payload]
    _push_payload_userdata(L, payload, payload_len);        // [table][payload][metatable]
    lua_settable(L, -3);                                    // [table]
    // clang-format on
}

static void lua_model_error(lua_State* L, const char* msg)
{
    if (__log_level__ != LOG_QUIET)
        log_error("Lua Error: %s (%s)", msg, lua_tostring(L, -1));
}


int pdunet_lua_pdu_call(
    lua_State* L, int32_t func_ref, uint8_t* payload, uint32_t payload_len)
{
    if (L == NULL) return -EINVAL;
    if (payload == NULL) return -EINVAL;

    lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return -EINVAL;
    }
    _lua_push_pdu_ctx(L, payload, payload_len);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_model_error(L, "lua_pcall() failed");
        lua_pop(L, 1);
        return -1;
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    int idx = lua_gettop(L);
    // Check the ctx table for an err.
    int err = 0;
    lua_pushstring(L, "err");
    lua_gettable(L, idx);
    if (lua_isinteger(L, -1)) {
        err = (int)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    if (err) {
        lua_pushstring(L, "errmsg");
        lua_gettable(L, idx);
        if (lua_isstring(L, -1)) {
            const char* msg = lua_tostring(L, -1);
            if (msg) {
                if (__log_level__ != LOG_QUIET)
                    log_error("lua call returned error: %s (%d)", msg, err);
            }
        }
        lua_pop(L, 2);
        return -1;
    }

    lua_pop(L, 1);
    return 0;
}

static void _lua_push_signal_ctx(lua_State* L, double phys, uint64_t raw,
    uint8_t* payload, uint32_t payload_len)
{
    // clang-format off
    lua_newtable(L);                                        // [table]
    lua_pushstring(L, "phys");                              // [table][phys]
    lua_pushnumber(L, phys);                                // [table][phys][double]
    lua_settable(L, -3);                                    // [table]
    lua_pushstring(L, "raw");                               // [table][raw]
    lua_pushinteger(L, (lua_Integer)raw);                   // [table][raw][uint64]
    lua_settable(L, -3);                                    // [table]
    lua_pushstring(L, "payload");                           // [table][payload]
    _push_payload_userdata(L, payload, payload_len);        // [table][payload][metatable]
    lua_settable(L, -3);                                    // [table]
    // clang-format on
}

int pdunet_lua_signal_call(lua_State* L, int32_t func_ref, double* phys,
    uint64_t* raw, uint8_t* payload, uint32_t payload_len)
{
    if (L == NULL) return -EINVAL;
    if (phys == NULL) return -EINVAL;
    if (raw == NULL) return -EINVAL;
    if (payload == NULL) payload_len = 0;

    lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return -EINVAL;
    }
    _lua_push_signal_ctx(L, *phys, *raw, payload, payload_len);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_model_error(L, "lua_pcall() failed");
        lua_pop(L, 1);
        return -1;
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    int idx = lua_gettop(L);

    // Check the ctx table for an err.
    int err = 0;
    lua_pushstring(L, "err");
    lua_gettable(L, idx);
    if (lua_isinteger(L, -1)) {
        err = (int)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    if (err) {
        lua_pushstring(L, "errmsg");
        lua_gettable(L, idx);
        if (lua_isstring(L, -1)) {
            const char* msg = lua_tostring(L, -1);
            if (msg) {
                if (__log_level__ != LOG_QUIET)
                    log_error("lua call returned error: %s (%d)", msg, err);
            }
        }
        lua_pop(L, 2);
        return -1;
    }

    // Check if values have changed.
    lua_pushstring(L, "raw");
    lua_gettable(L, idx);
    if (lua_isinteger(L, -1)) {
        *raw = (uint64_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    lua_pushstring(L, "phys");
    lua_gettable(L, idx);
    if (lua_isnumber(L, -1)) {
        *phys = lua_tonumber(L, -1);
    }
    lua_pop(L, 2);
    return 0;
}


void pdunet_load_lua_func(YamlNode* n, const char* path, const char** out)
{
    *out = NULL;
    YamlNode* np = dse_yaml_find_node(n, path);
    if (np) {
        for (uint32_t i = 0; i < hashlist_length(&np->sequence); i++) {
            YamlNode* seq = hashlist_at(&np->sequence, i);
            if (dse_yaml_get_string(seq, "lua", out) == 0) {
                break;
            }
        }
    }
}


const char* pdunet_build_func_name(PduNetworkDesc* net, PduItem* pdu,
    PduSignalItem* signal, const char* prefix)
{
    assert(net);
    assert(net->name);
    assert(prefix);
    if (pdu == NULL) return NULL;
    assert(pdu->name);

    static char b[MAX_URI_LEN];
    int         len;

    if (signal == NULL) {
        len =
            snprintf(b, sizeof(b), "%s__%s__%s", prefix, net->name, pdu->name);
    } else {
        assert(signal->name);
        len = snprintf(b, sizeof(b), "%s__%s__%s__%s", prefix, net->name,
            pdu->name, signal->name);
    }
    if (len < 0 || len >= (int)sizeof(b)) {
        log_error("Lua PDUNET function name truncated (exceeds %zu chars): %s",
            sizeof(b), b);
    }
    for (size_t i = 0; i < sizeof(b) && b[i]; i++) {
        if (!isalnum((unsigned char)b[i]) && b[i] != '_') {
            b[i] = '_';
        }
    }
    return b;
}


int pdunet_lua_install_func(
    lua_State* L, const char* func_name, const char* lua_script)
{
    assert(L);
    // log_trace("Install Lua function: %s\n%s", func_name, lua_script);

    /* Run the Lua script. */
    int top = lua_gettop(L);
    if (lua_script != NULL && luaL_dostring(L, lua_script) != 0) {
        log_error(
            "Lua Error: luaL_dostring() failed (%s)", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }
    lua_settop(L, top);

    /* Attempt to locate the function, which may not exist in the script, but
    might already exist in the Lua runtime (i.e. already installed). */
    if (func_name != NULL) {
        lua_getglobal(L, func_name);
        if (!lua_isfunction(L, -1)) {
            log_trace("Lua Error: Not a function (%s)", func_name);
            lua_pop(L, 1);
            /* The function was not found, normal condition. */
            return 0;
        }
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        if (ref < 0) {
            log_error(
                "Failed to create reference for function '%s'", func_name);
            return 0;
        }
        log_notice("PDU Net: Lua function installed: %s (ref=%d)\n%s",
            func_name, ref, lua_script);
        return ref;
    } else {
        /* No function was requested. */
        return 0;
    }
}


void pdunet_parse_network_functions(PduNetworkDesc* net)
{
    assert(net);

    // Parse spec/functions.
    YamlNode* n = dse_yaml_find_node(net->doc, "spec/functions");
    if (n != NULL) {
        pdunet_load_lua_func(n, "global", &net->lua.global);
    }
}


int pdunet_lua_setup(PduNetworkDesc* net)
{
    assert(net);
    assert(net->mi);
    assert(net->mi->private);
    ModelInstancePrivate* mip = net->mi->private;

    /* Establish the Lua interpreter. */
    if (mip->lua_state == NULL) {
        mip->lua_state = lua_model_create(NULL);
        // FIXME: do we have a model object at this point ???
        assert(mip->lua_state);
    }

    lua_State* L = mip->lua_state;
    _create_payload_metatable(L);

    return 0;
}


void pdunet_lua_teardown(PduNetworkDesc* net)
{
    if (net == NULL || net->mi == NULL) return;

    ModelInstancePrivate* mip = net->mi->private;
    if (mip && mip->lua_state != NULL) {
        lua_model_destroy(mip->lua_state);
        mip->lua_state = NULL;
    }
}
