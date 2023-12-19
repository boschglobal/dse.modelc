#! /usr/bin/python
# Copyright (C) 2023 Robert Bosch GmbH
import os
import asyncio
import time
from prettytable import PrettyTable
import statistics
import csv
import argparse

TIMEOUT = 6000
VALGRIND_CMD = 'valgrind --track-origins=yes --leak-check=full --error-exitcode=808'
USE_VALGRIND = False
CWD = os.getcwd() + "/"

# Sandbox for Model C
MODELC_SANDBOX_DIR = CWD + 'dse/modelc/build/_out'
SIMBUS_EXE = MODELC_SANDBOX_DIR+'/bin/simbus'
MODELC_EXE = MODELC_SANDBOX_DIR+'/bin/modelc'

# MODEL_YAML = 'data/model.yaml'
MODEL_YAML = CWD + 'tests/pytest/benchmark/model.yaml'
# signal_gen.generate_signal_group_yaml(CWD,2)
SIGNAL_GROUP_YAML = CWD + 'tests/pytest/benchmark/signal_group.yaml' # 'data/signal_group.yaml'
# SIGNAL_GROUP_YAML = 'data/signal_group.yaml'

BENCHMARK_RESULT = {}
TCP_TRANSPORT = 'redis://redis:6379'
TRANSPORT = os.environ['SIMBUS_URI'] # transport from docker-compose env variable, change this in docker-compose to update the transport type.
SIGNAL_CHANGE = os.environ['SIGNAL_CHANGE'] # signal change count

parser = argparse.ArgumentParser()
parser.add_argument("--stepsize", nargs='?', default=os.getenv('STEP_SIZE', '0.005'), help="Step Size")
parser.add_argument("--endtime", nargs='?', default=os.getenv('END_TIME', '10'), help="End Time")
parser.add_argument("--outfile", nargs='?', default=os.getenv('OUT_FILE', 'benchmark_result.csv'), help="filename of generated csv file")
parser.add_argument("--num_of_models", nargs='?', default=os.getenv('NUM_OF_MODELS', '2'), help="Number of Models")
args = parser.parse_args()

STEP_SIZE = args.stepsize
END_TIME = args.endtime
OUT_FILE = args.outfile
NUM_OF_MODELS = args.num_of_models
STACK_YAML = CWD + "tests/pytest/benchmark/stack.yaml" # Default stack yaml
SIGNAL_COUNT = os.getenv('SIGNAL_COUNT', 1)
MODEL_INSTANCE_NAMES = os.getenv('MODEL_INSTANCE_NAMES', 'dynamic_model_instance_1;dynamic_model_instance_2')


def form_model_instance_param_data(MODEL_INSTANCE_NAMES):
    INDIVIDUAL_MODEL_INSTANCE_MODELS, STACKED_MODEL_INSTANCE_MODEL_INSTANCES = [], ''
    for model_instance_name in MODEL_INSTANCE_NAMES.split(';'):
        instance_dict = {
                    'MODEL_INST': model_instance_name,
                    'MODEL_YAML_FILES': f'{MODEL_YAML} ' + f'{STACK_YAML} ' + f'{SIGNAL_GROUP_YAML}',
                }
        INDIVIDUAL_MODEL_INSTANCE_MODELS.append(instance_dict)
    STACKED_MODEL_INSTANCE_MODEL_INSTANCES = '"' + MODEL_INSTANCE_NAMES + '"'
    return INDIVIDUAL_MODEL_INSTANCE_MODELS, STACKED_MODEL_INSTANCE_MODEL_INSTANCES


INDIVIDUAL_MODEL_INSTANCE_MODELS, STACKED_MODEL_INSTANCE_MODEL_INSTANCES = form_model_instance_param_data(MODEL_INSTANCE_NAMES)


async def run(dir, cmd):
    _cmd = f'cd {dir}; {VALGRIND_CMD if USE_VALGRIND else ""} {cmd}'
    print('Run:')
    print(f'  dir: {dir}')
    print(f'  cmd: {_cmd}')
    p = await asyncio.create_subprocess_shell(
        _cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    stdout, stderr = await p.communicate()
    return { 'dir': dir, 'cmd': _cmd, 'rc': p.returncode, 'stdout': stdout.decode(), 'stderr': stderr.decode() }


async def main(params, checks):
    run_list = []
    transport_uri_str = f'--uri {TRANSPORT} ' if TRANSPORT != TCP_TRANSPORT else ''

    # SimBus
    run_list.append(asyncio.wait_for(run(
            params['MODEL_SANDBOX_DIR'], f'{SIMBUS_EXE} --logger 5 ' + f'--stepsize {STEP_SIZE} ' + f'--transport redispubsub ' + transport_uri_str + f'--timeout 1 {STACK_YAML}'), timeout=TIMEOUT)
    )
    # Models
    for m in params['models']:
        run_list.append(asyncio.wait_for(run(
            params['MODEL_SANDBOX_DIR'], f'{MODELC_EXE} --logger 5 ' + f'--name {m["MODEL_INST"]} ' + f'{m["MODEL_YAML_FILES"]} ' + f'--stepsize {STEP_SIZE} ' + f'--transport redispubsub ' + transport_uri_str + f'--endtime {END_TIME}'), timeout=TIMEOUT))
    # Gather results.
    result_list = await asyncio.gather(*run_list)

    for result in result_list:
        print('************************************************************')
        print('************************************************************')
        print('************************************************************')
        print(f'CMD : {result["cmd"]}')
        print('++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++')
        print(f'STDOUT : \n{result["stdout"]}')
        print('++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++')
        print(f'STDERR : \n{result["stderr"]}')
        print('++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++')
        print(f'RC : {result["rc"]}')
        print('************************************************************')
        print('')
        assert result['rc'] == 0

    for check in checks:
        # Search for check in each result STDOUT
        found = False
        for result in result_list:
            if check in result['stdout']:
                found = True
        assert found, check


def exec_time(func_name, key, value):
    if func_name in BENCHMARK_RESULT:
        dct_obj = BENCHMARK_RESULT[func_name]
        dct_obj[key].append(value)
    else :
        BENCHMARK_RESULT[func_name] = { key : [value] } # time_in_sec


def generate_csv(csv_file_data, out_path):
    if os.path.isdir(out_path) is False :
        os.mkdir(out_path)
    with open(out_path + '/' + OUT_FILE, 'a', newline='') as csvfile:
        csvwriter = csv.writer(csvfile, delimiter=',')
        for row in csv_file_data:
            csvwriter.writerow(row)


def display_benchmark_result():
    out_path = CWD + "tests/pytest/benchmark/_out"
    log_table = PrettyTable()
    print('================================================ Benchmark Test Result ================================================')
    field_names = ["scenario", "transport", "models", "step size", "end time", "signals", "signal change" , "run time", "rate (steps/sec)", "factor ()"]
    log_table.field_names = field_names
    csv_file_data = [] if os.path.isfile(out_path + '/' + OUT_FILE) else [field_names] # adding csv header data
    for func in BENCHMARK_RESULT:
        exec_time_list = BENCHMARK_RESULT[func]['runtime']
        # min_runtime = "{:.6f}".format(min(exec_time_list))
        mean_runtime = "{:.6f}".format(statistics.mean(exec_time_list))
        # median_runtime = "{:.6f}".format(statistics.median(exec_time_list))
        # max_runtime = "{:.6f}".format(max(exec_time_list))
        sim_step_per_second = "{:.0f}".format((float(END_TIME) / float(STEP_SIZE)) / float(mean_runtime))
        speed = "{:.4f}".format(float(END_TIME) / float(mean_runtime))
        transport = 'redispubsub_tcp' if TRANSPORT == TCP_TRANSPORT else 'redispubsub_socket'
        row = [func, transport, NUM_OF_MODELS, STEP_SIZE, END_TIME, SIGNAL_COUNT , SIGNAL_CHANGE, mean_runtime, sim_step_per_second, speed]
        csv_file_data.append(row)
        log_table.add_row(row)
    generate_csv(csv_file_data, out_path)
    print(log_table)


def timer_func(func):
    def function_timer():
        start = time.time()
        value = func()
        end = time.time()
        runtime = end - start
        exec_time(func.__name__, 'runtime', runtime)
        return value
    return function_timer


@timer_func
def test_individual_model_instance():
    # Two models executed by the two instances of ModelC.
    params = {
        'MODEL_SANDBOX_DIR': MODELC_SANDBOX_DIR + "/examples/benchmark",
        'models': INDIVIDUAL_MODEL_INSTANCE_MODELS
    }
    checks = []
    asyncio.run(main(params, checks))


@timer_func
def test_stacked_model_instance():
    # Two models executed by the same instance of ModelC.
    params = {
        'MODEL_SANDBOX_DIR': MODELC_SANDBOX_DIR + '/examples/benchmark',
        'models': [
            {
                'MODEL_INST': STACKED_MODEL_INSTANCE_MODEL_INSTANCES,
                'MODEL_YAML_FILES': f'{MODEL_YAML} ' + f'{STACK_YAML} ' + f'{SIGNAL_GROUP_YAML}',
            },
        ]
    }
    checks = []
    asyncio.run(main(params, checks))


if __name__ == "__main__":
    test_stacked_model_instance()
    test_individual_model_instance()
    display_benchmark_result()
