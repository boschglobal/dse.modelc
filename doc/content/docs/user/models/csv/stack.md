
---
kind: Stack
metadata:
  name: csv_stack
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
    - name: input
      uid: 42
      model:
        name: dse.modelc.csv
      runtime:
        env:
          CSV_FILE: data/valueset.csv
      channels:
        - name: physical
          alias: signal_channel
---
kind: Model
metadata:
  name: simbus
---
kind: SignalGroup
metadata:
  name: signal_vector
  labels:
    channel: signal_vector
spec:
  signals:
    - signal: A
    - signal: B
    - signal: C
