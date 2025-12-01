```hs
simulation
channel physical

uses
dse.modelc https://github.com/boschglobal/dse.modelc v2.1.32

model input dse.modelc.csv
channel physical signal_channel
envar CSV_FILE model/input/data/valueset.csv
file valueset.csv input.csv
```