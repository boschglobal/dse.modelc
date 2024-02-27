# ModelC Benchmark Test

## Host Setup

```bash
# Install Python packages.
$ pip install -r requirements.txt

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

# Setup benchmark result folder
$ mkdir tests/pytest/benchmark/_out
$ sudo chmod 777 tests/pytest/benchmark/_out

# Run the benchmark.
$ cd tests/pytest/benchmark
$ cp scenario/redis_socket_6_400.csv input.csv

## Method 1 (takes default values)
$ make

## Method 2
$ ./benchmark.py execute --transport redis://redis:6379 --scenario 'input.csv' --topology stacked --outdir '_out' --signals 15 --signal_change 5 --channels 3 --models 5 --step_size .05 --end_time 1


# Results
$ ls -al tests/pytest/benchmark/_out
```


## Generate static yaml files

```bash
$ ./benchmark.py  generate --models 1 --signals 10 --transport posix:///stem --channels 2 --outdir 'output/'
```

## Configuration

### Input file setup
    File Path : dse.modelc/tests/pytest/benchmark/
    File Name : input.csv
    File Type : csv
    Columns :
      - simbus_uri (defines the simbus transport type)
      - num_of_channels (number of channels)
      - num_of_models (number of models)
      - step_size (step size)
      - end_time (end time)
      - signal_count (total number signals to be generated in signal_group.yaml)
      - signal_change (max range of signals involved in simulation)
      - out_file (file name for storing benchmark result. expected file type : csv)
