#include "network.hpp"
#include "simulator.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <fmt/format.h>
#include <fmt/ostream.h>

using namespace caspian;

void run_test(int inputs, int outputs, int hidden, int runs, int seed, int runtime = 0)
{
    int n_neurons = inputs + outputs + hidden;
    uint64_t accumulations = 0;

    std::vector<std::chrono::duration<double>> sim_times;

    int n_input_synapses = std::min(hidden, 64);
    int n_output_synapses = std::min(hidden, 64);
    int n_hidden_synapses = std::min(hidden, 16);
    int n_hidden_synapses_max = n_hidden_synapses * 2;

    auto rand_start = std::chrono::system_clock::now();

    Network net(n_neurons);

    // Generate the pass network
    net.make_random(inputs, outputs, seed, 
                    n_input_synapses,
                    n_output_synapses,
                    n_hidden_synapses,
                    n_hidden_synapses_max);

    // Configure the simulator with the new network
    auto cfg_start = std::chrono::system_clock::now();

    Simulator sim;
    sim.configure(&net);

    auto cfg_end = std::chrono::system_clock::now();

    std::chrono::duration<double, std::micro> rnd_duration = (cfg_start - rand_start);
    std::chrono::duration<double, std::micro> cfg_duration = (cfg_end - cfg_start);

    fmt::print("Seed: {} | Inputs: {} Outputs: {} | Neurons: {} Synapses: {} | Cycles: {}\n", 
            seed, inputs, outputs, net.num_neurons(), net.num_synapses(), runtime);
    fmt::print("Random Net: {} us\n", rnd_duration.count()); 
    fmt::print("Configure : {} us\n", cfg_duration.count());

    for(int r = 0; r < runs; ++r)
    {
        auto sim_start = std::chrono::system_clock::now();

        // Queue up inputs
        for(int i = 0; i < inputs; ++i)
        {
            sim.apply_input(i, 500, i);
        }

        // Simulate with sufficient time (intentionally extra)
        sim.simulate(runtime);
        accumulations = sim.get_metric("accumulate_count");
        auto sim_end = std::chrono::system_clock::now();

        std::chrono::duration<double> sim_time = sim_end - sim_start;
        std::cout << "Simulate " << r << ": " << (sim_time).count() << " s" << std::endl;
        sim_times.push_back(sim_time);

        sim.clear_activity();
    }

    std::sort(sim_times.begin(), sim_times.end());

    double avg = 0;
    for(auto const &t : sim_times) avg += t.count();
    avg /= sim_times.size();
    fmt::print("Average Simulate (s)     : {}\n", avg);
    fmt::print("Median Simulate  (s)     : {}\n", sim_times[sim_times.size()/2].count());
    fmt::print("Accumulations            : {}\n", accumulations);
    fmt::print("Accumulations per second : {:.1f}\n", static_cast<double>(accumulations) / avg);
    fmt::print("Effective Clock Speed    : {:.4f} KHz\n", (static_cast<double>(runtime) / avg) / (1000) );
}

int main(int argc, char **argv)
{
    int inputs, outputs, hidden, runs, rt, seed;

    if(argc < 7)
    {
        fmt::print("Usage: {} inputs outputs hidden n_runs runtime seed\n", argv[0]);
        exit(1);
    }

    inputs = atoi(argv[1]);
    outputs = atoi(argv[2]);
    hidden = atoi(argv[3]);
    runs = atoi(argv[4]);
    rt = atoi(argv[5]);
    seed = atoi(argv[6]);

    run_test(inputs, outputs, hidden, runs, seed, rt);
    return 0;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
