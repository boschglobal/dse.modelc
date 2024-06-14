#!/bin/bash

individual_model_input_files="individual_model_simer_redis_socket_4_1000.csv individual_model_simer_redis_socket_4_2000.csv individual_model_simer_redis_socket_4_4000.csv individual_model_simer_redis_socket_4_8000.csv individual_model_simer_redis_socket_4_16000.csv"
stacked_model_input_files="stacked_model_simer_redis_socket_4_1000.csv stacked_model_simer_redis_socket_4_2000.csv stacked_model_simer_redis_socket_4_4000.csv stacked_model_simer_redis_socket_4_8000.csv stacked_model_simer_redis_socket_4_16000.csv"

run () {
   files=($@)
   topology=${files[-1]}
   unset files[-1] # removes the toplogy value from filenames array
   for i in "${files[@]}"; do
        echo "Running benchmark test for the file $i..."   
        cp performance_improvement_test/$i input.csv
        ./benchmark.py execute --topology $topology --runtime simer
   done
}

run $individual_model_input_files individual
run $stacked_model_input_files stacked