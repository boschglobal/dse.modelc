import yaml
import re
import os
import argparse


def func_replace(file_data, return_type, func_name, args, replace_content):
    pattern = return_type + r'\s+' + func_name + r'\(?' + args + r'\)?\n?\s*\{\n((?:.*?|\n)*?)\}\s*(\w+\;)?'
    data = re.sub(pattern, replace_content, file_data, flags=re.DOTALL | re.M)
    return data


def generate_signal_group_yaml(CWD, sample_count):
    signals = ['SIGNAL_' + str(i) for i in range(sample_count)]
    print('Signal Count : ', len(signals))

    signal_group_dict = [{'kind' : 'SignalGroup'}, {'metadata' : [{'name' : 'test_signals'}, {'labels' : [{'channel' : 'test'}, {'model' : 'benchmark_model'}]}]}, {'spec' : [{'signals' : []}]}]
    # sample_signals = random.sample(signals,sample_count)
    signal_group_dict[2]['spec'][0]['signals'].extend(signals)

    signal_name_index = '' # for replacing the 'signal_name_index' enum content in benchmark_model.c
    model_function_do_step = '' # for replacing the 'MODEL_FUNCTION_DO_STEP' function benchmark_model.c
    for signal in signals:
        signal = signal.replace('signal: ', '')
        signal_name_index += signal + ',\n'
        model_function_do_step += 'signal_value[' + signal + '] += 1.2;\n'

    signal_name_index = "typedef enum signal_name_index {\n" + \
                        signal_name_index + \
                        "__SIGNAL__COUNT__\n" +   \
                        "} signal_name_index;\n\n\n"
    model_function_do_step = "int MODEL_FUNCTION_DO_STEP(double* model_time, double stop_time){\n" + "assert(signal_value);\n" + model_function_do_step + "*model_time = stop_time;\n" + "return 0; \n} "

    # Generating signal_group yaml file
    with open(CWD + 'tests/pytest/benchmark/_working/signal_group.yaml', 'w') as file:
        yaml.dump(signal_group_dict, file)
    with open(CWD + 'tests/pytest/benchmark/_working/signal_group.yaml', 'r') as fp:
        data = fp.read()
        typos = re.sub(r'\- ', '', data, count=8)
        typos = re.sub(r'\'', '', typos)
    with open(CWD + 'tests/pytest/benchmark/_working/signal_group.yaml', 'w') as fp:
        fp.write(typos)


def main(num_of_signals):
    modelc_path = '/'.join(os.getcwd().split('/')[:-3]) + '/'
    generate_signal_group_yaml(modelc_path , int(num_of_signals))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--num_of_signals", nargs='?', default=2, help="Number of Signals")
    args = parser.parse_args()
    main(args.num_of_signals)
