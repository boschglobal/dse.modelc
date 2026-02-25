// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_MODEL_PDUNET_NETWORK_H_
#define DSE_MODELC_MODEL_PDUNET_NETWORK_H_

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <lua.h>
#include <lauxlib.h>
#include <dse/platform.h>
#include <dse/logger.h>
#include <dse/clib/collections/vector.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/model/lua.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/pdunet.h>
#include <dse/ncodec/interface/pdu.h>


#define NONE SchemaFieldTypeNONE
#define S    SchemaFieldTypeS
#define D    SchemaFieldTypeD
#define B    SchemaFieldTypeB
#define U8   SchemaFieldTypeU8
#define U16  SchemaFieldTypeU16
#define U32  SchemaFieldTypeU32


/* network.c */
DLL_PRIVATE uint32_t pdunet_checksum(const uint8_t* payload, size_t len);
DLL_PRIVATE void     pdunet_schedule(PduNetworkDesc* net);
DLL_PRIVATE int      pdunet_parse(PduNetworkDesc* net, SchemaLabel* labels);
DLL_PRIVATE void     pdunet_build_msm(PduNetworkDesc* net, const char* sv_name);
DLL_PRIVATE int      pdunet_configure(PduNetworkDesc* net);
DLL_PRIVATE int pdunet_transform(PduNetworkDesc* net, PduNetworkSortFunc sort);
DLL_PRIVATE PduNetworkNCodecVTable pdunet_network_factory(PduNetworkDesc* net);
DLL_PRIVATE void pdunet_parse_pdus(PduNetworkDesc* net, SchemaObject* object);
DLL_PRIVATE PduItem pdunet_pdu_generator(PduNetworkDesc* net, YamlNode* n);
DLL_PRIVATE PduSignalItem pdunet_signal_generator(
    ModelInstanceSpec* mi, YamlNode* n, PduItem* pdu);

/* matrix.c */
DLL_PRIVATE int pdunet_matrix_transform(
    PduNetworkDesc* net, PduNetworkSortFunc sort);
DLL_PRIVATE void pdunet_matrix_clear(PduNetworkDesc* net);
DLL_PRIVATE void pdunet_pdu_calculate_linear_range(
    PduNetworkDesc* net, PduRange* r);
DLL_PRIVATE void pdunet_pdu_pack_range(PduNetworkDesc* net, PduRange* r);

DLL_PRIVATE void pdunet_encode_linear(PduNetworkDesc* net, PduRange* range);
DLL_PRIVATE void pdunet_decode_linear(PduNetworkDesc* net, PduRange* range);
DLL_PRIVATE void pdunet_encode_pack(PduNetworkDesc* net, PduRange* range);
DLL_PRIVATE void pdunet_decode_unpack(PduNetworkDesc* net, PduRange* range);

/* lua.c */
DLL_PRIVATE void pdunet_parse_network_functions(PduNetworkDesc* net);
DLL_PRIVATE void pdunet_load_lua_func(
    YamlNode* n, const char* path, const char** out);

DLL_PRIVATE int         pdunet_lua_setup(PduNetworkDesc* net);
DLL_PRIVATE const char* pdunet_build_func_name(PduNetworkDesc* net,
    PduItem* pdu, PduSignalItem* signal, const char* prefix);
DLL_PRIVATE int         pdunet_lua_install_func(
            lua_State* L, const char* func_name, const char* lua_script);
DLL_PRIVATE int pdunet_lua_pdu_call(
    lua_State* L, int32_t func_ref, uint8_t* payload, uint32_t payload_len);
DLL_PRIVATE int  pdunet_lua_signal_call(lua_State* L, int32_t func_ref,
     double* phys, uint64_t* raw, uint8_t* payload, uint32_t payload_len);
DLL_PRIVATE void pdunet_lua_teardown(PduNetworkDesc* net);

/* flexray.c */
DLL_PRIVATE void pdunet_flexray_parse_network(PduNetworkDesc* net);
DLL_PRIVATE void pdunet_flexray_parse_pdu(PduItem* pdu, void* n);
DLL_PRIVATE void pdunet_flexray_config(PduNetworkDesc* net);
DLL_PRIVATE void pdunet_flexray_lpdu_tx(PduNetworkDesc* net);
DLL_PRIVATE void pdunet_flexray_lpdu_rx(PduNetworkDesc* net);
DLL_PRIVATE void pdunet_flexray_status(PduNetworkDesc* net);
DLL_PRIVATE void pdunet_flexray_parse_network_metadata(
    PduNetworkDesc* net, YamlNode* md);
DLL_PRIVATE void pdunet_flexray_parse_pdu_metadata(PduItem* pdu, YamlNode* md);


#endif  // DSE_MODELC_MODEL_PDUNET_NETWORK_H_
