// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_MODEL_LUA_H_
#define DSE_MODELC_MODEL_LUA_H_

#include <lua.h>
#include <dse/modelc/model.h>
#include <dse/modelc/mcl.h>
#include <dse/platform.h>


typedef struct LuaMclModel {
    MclDesc mcl;

    /* Lua MCL properties. */
    lua_State*  L; /* Lua State object (shadows mi->lua_state). */
    const char* lua_model_path;
} LuaMclModel;


/* lua.c */
DLL_PRIVATE lua_State* lua_model_create(ModelDesc* m);
DLL_PRIVATE void       lua_model_destroy(lua_State* L);
DLL_PRIVATE MclDesc*   lua_mcl_create(ModelDesc* model);
DLL_PRIVATE void       lua_mcl_destroy(MclDesc* model);

/* luamodel.c */
DLL_PRIVATE int lua_modellib_open(lua_State* L, ModelDesc* m);


#endif  // DSE_MODELC_MODEL_LUA_H_
