---
title: "Model - CSV"
linkTitle: "CSV"
weight: 100
tags:
- Model
- CSV
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## Synposis

A model for setting simulation signals with values read from a CSV file.


## Model

### Structure

```text
examples/csv
    └── sim
        ├── data
        │    └── simulation.yml
        └── model
            └── input
                └── lib/libcsv.so
                └── data
                    └── model.yml
                    └── valueset.csv    <-- CSV File.
```

### CSV File

```csv
Timestamp;A;B;C
0.0000;1.0;2.0;3.0
0.0005;-1.1;2.1;3.1
0.0010;1.2;-2.2;3.2
0.0015;1.3;2.3;-3.3
```

## Examples

### Simulation / DSE Script

{{ readFile "script.md" chomp }}

### Simulation / Stack

```yaml
{{ readFile "stack.md" chomp }}
```
