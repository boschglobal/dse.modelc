# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

###############
## Docker Images.
DSE_CDOCGEN_IMAGE ?= dse-cdocgen:latest
DSE_PLANTUML_IMAGE ?= dse-plantuml:latest


###############
## Document Modules

# File "_index.md"
define _INDEX_MD_FILE =
---
title: "ModelC API Reference"
linkTitle: "ModelC"
weight: 80
---

## ModelC API Reference
endef
export _INDEX_MD_FILE
DOC_OUTPUT_index := doc/content/apis/modelc/_index.md

# Module "gateway"
DOC_INPUT_gateway := dse/modelc/gateway.h
DOC_CDIR_gateway := dse/modelc/model/gateway.c
DOC_OUTPUT_gateway := doc/content/apis/modelc/gateway/index.md
DOC_LINKTITLE_gateway := Gateway
DOC_TITLE_gateway := "Gateway API Reference"

# Module "mcl"
DOC_INPUT_mcl := dse/modelc/mcl.h
DOC_CDIR_mcl := dse/modelc/controller/mcl.c
DOC_OUTPUT_mcl := doc/content/apis/modelc/mcl/index.md
DOC_LINKTITLE_mcl := MCL
DOC_TITLE_mcl := "MCL API Reference"

# Module "model"
DOC_INPUT_model := dse/modelc/model.h
DOC_CDIR_model := dse/modelc/model/model.c,dse/modelc/model/signal.c
DOC_OUTPUT_model := doc/content/apis/modelc/model/index.md
DOC_LINKTITLE_model := Model
DOC_TITLE_model := "Model API Reference"

# Module "runtime"
DOC_INPUT_runtime := dse/modelc/runtime.h
DOC_CDIR_runtime := dse/modelc/controller/modelc.c,dse/modelc/controller/modelc_args.c,dse/modelc/controller/modelc_debug.c,
DOC_OUTPUT_runtime := doc/content/apis/modelc/runtime/index.md
DOC_LINKTITLE_runtime := Runtime
DOC_TITLE_runtime := "Runtime API Reference"

# Module "schema"
DOC_INPUT_schema := dse/modelc/schema.h
DOC_CDIR_schema := dse/modelc/model/schema.c
DOC_OUTPUT_schema := doc/content/apis/modelc/schema/index.md
DOC_LINKTITLE_schema := Schema
DOC_TITLE_schema := "Schema API Reference"

# Module "simmock"
DOC_INPUT_simmock := dse/mocks/simmock.h
DOC_CDIR_simmock := dse/mocks/simmock.c
DOC_OUTPUT_simmock := doc/content/apis/modelc/simmock/index.md
DOC_LINKTITLE_simmock := SimMock
DOC_TITLE_simmock := "SimMock API Reference"


# Targets
DOC_C_MODULES := gateway mcl model runtime schema simmock
#DOC_C_MODULES :=  model

.PHONY: examples
examples:
# API Examples
	cd ..;mkdir -p doc/content/apis/modelc/examples
	cd ..;cp dse/modelc/examples/gateway/gateway.c doc/content/apis/modelc/examples/gateway.c
	cd ..;cp dse/modelc/examples/doc/apis/*.c doc/content/apis/modelc/examples
# DOC Examples
	cd ..;mkdir -p doc/content/docs/examples/modelc/mocks
	cd ..;find dse/mocks/examples/* -type d | xargs -I % cp -r % doc/content/docs/examples/modelc/mocks
	cd ..;find dse/modelc/examples/doc/docs/* -type d | xargs -I % cp -r % doc/content/docs/examples/modelc

.PHONY: index
index:
	@cd ..; mkdir -p $$(dirname $(DOC_OUTPUT_index))
	@cd ..; echo "$$_INDEX_MD_FILE" > $(DOC_OUTPUT_index)

.PHONY: $(DOC_C_MODULES)
$(DOC_C_MODULES):
	@cd ..; mkdir -p $$(dirname $(DOC_OUTPUT_$@))
	@cd ..; docker run -it --rm \
		--volume $$(pwd):/tmp/repo \
		--workdir /tmp/repo \
		$(DSE_CDOCGEN_IMAGE) \
			--input $(DOC_INPUT_$@) \
			--output $(DOC_OUTPUT_$@) \
			--cdir $(DOC_CDIR_$@) \
			--title $(DOC_TITLE_$@) \
			--linktitle $(DOC_LINKTITLE_$@)
	@cd ../$$(dirname $(DOC_OUTPUT_$@)); docker run -it --rm \
		--volume $$(pwd):/tmp/repo \
		--workdir /tmp/repo \
		$(DSE_PLANTUML_IMAGE) \
			-tpng  $$(basename $(DOC_OUTPUT_$@))

.PHONY: generate
generate: index examples $(DOC_C_MODULES)
