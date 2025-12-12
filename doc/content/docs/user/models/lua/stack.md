---
kind: Stack
metadata:
  name: lua_model
spec:
  connection:
    transport:
      redispubsub:
        uri: redis://localhost:6379
        timeout: 60
  models:
    - name: simbus
      model:
        name: simbus
      channels:
        - name: physical
          expectedModelCount: 1
    - name: lua_inst
      uid: 42
      runtime:
        mcl: lua
        files:
          - data/model.lua
      channels:
        - name: physical
          alias: signal
          selectors:
            signal: signal
---
kind: SignalGroup
metadata:
  name: signal
  labels:
    signal: signal
  annotations:
    vector_name: signal
spec:
  signals:
    - signal: counter
      annotations:
        initial_value: 23