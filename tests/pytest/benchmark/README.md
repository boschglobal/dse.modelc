# ModelC Benchmark Test

## Host Setup

```bash
# Install Python packages.
$ pip install  openpyxl numpy ruamel.yaml pandas plotly

# Install Docker Compose.
$ sudo -E  curl -SL \
    https://github.com/docker/compose/releases/download/v2.23.3/docker-compose-linux-x86_64 \
    -o /usr/local/bin/docker-compose
$ sudo chmod 777 /usr/local/bin/docker-compose
```


## Run a benchmark.

```bash
# Clone the repo (if necessary).
$ git clone https://github.com/boschglobal/dse.modelc.git

# Build the framework and models.
$ cd dse.modelc
$ make

# Run the benchmark.
$ cd tests/pytest/benchmark
$ cp scenario/redis_6_400.csv input.csv
$ make

# Results
$ ls -al tests/pytest/benchmark/_out
```


## Configuration

### Input file setup
    File Path : dse.modelc/tests/pytest/benchmark/
    File Name : input.csv
    File Type : csv
    Columns :
      - simbus_uri (defines the simbus transport type)
      - num_of_models (number of models)
      - step_size (step size)
      - end_time (end time)
      - signal_count (total number signals to be generated in signal_group.yaml)
      - signal_change (max range of signals involved in simulation)
      - out_file (file name for storing benchmark result. expected file type : csv)
