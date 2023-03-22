<!--
Copyright 2023 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

### Run the Example

##### Terminal 1: Redis

```bash
$ docker run --rm -it --name redis -p 6379:6379 redis &
```

##### Terminal 2: Standalone SimBus

```bash
$ export SIMBUS_EXE=$(pwd)/libmodelcapi/build/_out/bin/simbus
$ export MODEL_SANDBOX_DIR=$(pwd)/examples/dynamic_model/build/_out
$ export YAML_DIR=${MODEL_SANDBOX_DIR}/data/yaml/dynamic_model

$ cd ${MODEL_SANDBOX_DIR}
$ ${SIMBUS_EXE} --name simbus ${YAML_DIR}/dynamic_model.yaml
```

##### Terminal 3: Dynamically Linked Model

```bash
$ export MODELC_EXE=$(pwd)/libmodelcapi/build/_out/bin/modelc
$ export MODEL_SANDBOX_DIR=$(pwd)/examples/dynamic_model/build/_out
$ export YAML_DIR=${MODEL_SANDBOX_DIR}/data/yaml/dynamic_model

$ cd ${MODEL_SANDBOX_DIR}; \
     ${MODELC_EXE} --name dynamic_model_instance ${YAML_DIR}/dynamic_model.yaml; \
     cd -
```
