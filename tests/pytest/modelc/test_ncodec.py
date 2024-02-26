#! /usr/bin/python
# Copyright (C) 2021 Robert Bosch GmbH

import os
import asyncio
import pytest


pytestmark = pytest.mark.skipif(
        os.environ["PACKAGE_ARCH"] != "linux-amd64",
        reason="test only runs on linux-amd64")


TIMEOUT = 60
VALGRIND_CMD = 'valgrind --track-origins=yes --leak-check=full --error-exitcode=808'
USE_VALGRIND = True

# Sandbox for Model C
MODELC_SANDBOX_DIR = os.getenv('MODELC_SANDBOX_DIR')
SIMBUS_EXE = MODELC_SANDBOX_DIR+'/bin/simbus'
MODELC_EXE = MODELC_SANDBOX_DIR+'/bin/modelc'

# Sandbox for the Model (common only, specific in test cases).
MODEL_YAML = 'data/model.yaml'
STACK_YAML = 'data/simulation.yaml'


async def run(dir, cmd, env):
    _cmd = f'cd {dir}; {VALGRIND_CMD if USE_VALGRIND else ""} {cmd}'
    print('Run:')
    print(f'  dir: {dir}')
    print(f'  cmd: {_cmd}')
    print(f'  env: {env}')
    p = await asyncio.create_subprocess_shell(
        _cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        env=env,
    )
    stdout, stderr = await p.communicate()
    return { 'dir': dir, 'cmd': _cmd, 'rc': p.returncode, 'stdout': stdout.decode(), 'stderr': stderr.decode() }


async def main(params, checks, exclude_checks = None):
    run_list = []
    # SimBus
    run_list.append(asyncio.wait_for(
        run(
            params['MODEL_SANDBOX_DIR'],
            f'{SIMBUS_EXE} --logger 2 --timeout 1 {STACK_YAML}',
            None
        ),
        timeout=TIMEOUT)
    )
    # Models
    for m in params['models']:
        trace = f'{m["MCODEC_TRACE_NAME"]}={m["MCODEC_TRACE_NAME"]}'
        env = os.environ.copy()
        env[m["MCODEC_TRACE_NAME"]] = m["MCODEC_TRACE_VALUE"]
        run_list.append(asyncio.wait_for(
            run(
                params['MODEL_SANDBOX_DIR'],
                f'{MODELC_EXE} --logger 2 ' + f'--name {m["MODEL_INST"]} ' + f'{m["MODEL_YAML_FILES"]} ' + '',
                env
            ), timeout=TIMEOUT))
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

    if exclude_checks:
        for check in exclude_checks:
            # Search for check in each result STDOUT
            found = False
            for result in result_list:
                if check in result['stdout']:
                    found = True
            assert not found, check


def test_ncodec_trace_wildcard():
    params = {
        'MODEL_SANDBOX_DIR': os.getenv('MODELC_SANDBOX_DIR')+'/examples/ncodec',
        'models': [
            {
                'MODEL_INST': 'ncodec_inst',
                'MODEL_YAML_FILES' : f'{MODEL_YAML} ' + f'{STACK_YAML} ',
                'MCODEC_TRACE_NAME' : 'NCODEC_TRACE_CAN_1',
                'MCODEC_TRACE_VALUE' : '*',
            },
        ]
    }
    checks = [
        '(ncodec_inst) 0.000000 [1:24:3] TX 3e9 1 29 :',
        '48 65 6c 6c 6f 20 57 6f  72 6c 64 21 20 66 72 6f  6d 20 6e 6f 64 65 5f 69  64 3d 32 34 00',
        '(ncodec_inst) 0.005000 [1:24:3] RX 3e9 1 29 :',
        '(ncodec_inst) 0.005000 [1:24:3] TX 3ea 1 29 :',
        '(ncodec_inst) 0.010000 [1:24:3] RX 3ea 1 29 :',
        '(ncodec_inst) 0.010000 [1:24:3] TX 3eb 1 29 :',
        '(ncodec_inst) 0.015000 [1:24:3] RX 3eb 1 29 :',
        '(ncodec_inst) 0.015000 [1:24:3] TX 3ec 1 29 :',
        '(ncodec_inst) 0.020000 [1:24:3] RX 3ec 1 29 :',
        '(ncodec_inst) 0.020000 [1:24:3] TX 3ed 1 29 :',
    ]
    asyncio.run(main(params, checks))


def test_ncodec_trace_frame_list():
    params = {
        'MODEL_SANDBOX_DIR': os.getenv('MODELC_SANDBOX_DIR')+'/examples/ncodec',
        'models': [
            {
                'MODEL_INST': 'ncodec_inst',
                'MODEL_YAML_FILES' : f'{MODEL_YAML} ' + f'{STACK_YAML} ',
                'MCODEC_TRACE_NAME' : 'NCODEC_TRACE_CAN_1',
                'MCODEC_TRACE_VALUE' : '0x3ea,0x3eb'
            },
        ]
    }
    include_checks = [
        '(ncodec_inst) 0.005000 [1:24:3] TX 3ea 1 29 :',
        '(ncodec_inst) 0.010000 [1:24:3] RX 3ea 1 29 :',
        '(ncodec_inst) 0.010000 [1:24:3] TX 3eb 1 29 :',
        '(ncodec_inst) 0.015000 [1:24:3] RX 3eb 1 29 :',
    ]
    exclude_checks = [
        '(ncodec_inst) 0.000000 [1:24:3] TX 3e9 1 29 :',
        '(ncodec_inst) 0.005000 [1:24:3] RX 3e9 1 29 :',
        '(ncodec_inst) 0.015000 [1:24:3] TX 3ec 1 29 :',
        '(ncodec_inst) 0.020000 [1:24:3] RX 3ec 1 29 :',
        '(ncodec_inst) 0.020000 [1:24:3] TX 3ed 1 29 :',
    ]
    asyncio.run(main(params, include_checks, exclude_checks))
