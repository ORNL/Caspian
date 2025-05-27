#include "network.hpp"
#include <iostream>
#include <vector>
#include <chrono>
//#include <fmt/format.h>
//#include <fmt/ostream.h>

using namespace caspian;

void run_test(int inputs, int outputs, int hidden, int runs, int seed)
{
    int n_neurons = inputs + outputs + hidden;
    Network net(n_neurons);

    int n_input_synapses = std::min(hidden, 64);
    int n_output_synapses = std::min(hidden, 64);
    int n_hidden_synapses = std::min(hidden, 32);
    int n_hidden_synapses_max = n_hidden_synapses * 2;

    auto rand_start = std::chrono::system_clock::now();

    for(int i = 0; i < runs; ++i)
    {
        // Generate the pass network
        net.make_random(inputs, outputs,
                        seed + i, 
                        n_input_synapses,
                        n_output_synapses,
                        n_hidden_synapses,
                        n_hidden_synapses_max);
    }

    auto rand_end = std::chrono::system_clock::now();

    std::chrono::duration<double> total_duration = rand_end - rand_start;
    double total_time = total_duration.count();
    double avg_time = total_time / static_cast<double>(runs);

    printf("Total time   (s) : %lf\nAverage time (s) : %lf\n",total_time,avg_time);
    //fmt::print("Total time   (s) : {}\n", total_time);
    //fmt::print("Average time (s) : {}\n", avg_time);
}

int main(int argc, char **argv)
{
    int inputs, outputs, hidden, runs, seed;

    if(argc < 6)
    {
        printf("Usage: %s inputs outputs hidden n_runs seed\n", argv[0]);
        //fmt::print("Usage: {} inputs outputs hidden n_runs seed\n", argv[0]);
        exit(1);
    }

    inputs = atoi(argv[1]);
    outputs = atoi(argv[2]);
    hidden = atoi(argv[3]);
    runs = atoi(argv[4]);
    seed = atoi(argv[5]);

    run_test(inputs, outputs, hidden, runs, seed);
    return 0;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
