#include "network.hpp"
#include "simulator.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <fmt/format.h>
#include <fmt/ostream.h>

using namespace caspian;

void run_test(int inputs, int runs, int seed, int runtime, int input_time)
{
    std::mt19937 gen{seed};
    std::normal_distribution<> nd{0,1};
    const int max_weight = 255;
    const int threshold = 127;

    int n_neurons = inputs;
    uint64_t accumulations = 0;
    uint64_t fire_cnt = 0;
    uint64_t input_fire_cnt = 0;

    std::vector<std::chrono::duration<double>> sim_times;

    auto rand_start = std::chrono::system_clock::now();

    Network net(n_neurons);

    // Generate the network
    for(int i = 0; i < inputs; ++i)
    {
        net.add_neuron(i, threshold);
        net.set_input(i, i);
    }

    for(int i = 0; i < inputs; ++i)
    {
        for(int j = 0; j < inputs; ++j)
        {
            if(j == i) continue;

            int wd = std::round(nd(gen) * threshold);
            int w = std::max(-max_weight, std::min(max_weight, wd));

            net.add_synapse(i, j, w, 0);
        }
    }
    

    // Configure the simulator with the new network
    auto cfg_start = std::chrono::system_clock::now();

    Simulator sim;
    sim.configure(&net);

    auto cfg_end = std::chrono::system_clock::now();

    std::chrono::duration<double, std::micro> rnd_duration = (cfg_start - rand_start);
    std::chrono::duration<double, std::micro> cfg_duration = (cfg_end - cfg_start);

    fmt::print("Seed: {} | Neurons: {} Synapses: {} | Cycles: {} | Input Duration: {}\n", 
            seed, net.num_neurons(), net.num_synapses(), runtime, input_time);
    fmt::print("Random Net: {} us\n", rnd_duration.count()); 
    fmt::print("Configure : {} us\n", cfg_duration.count());

    for(int r = 0; r < runs; ++r)
    {
        std::uniform_int_distribution<> dis(1, 100);

        auto sim_start = std::chrono::system_clock::now();

        // Queue up inputs
        for(int i = 0; i < inputs; ++i)
        {
            int frate = dis(gen);
            for(int j = 0; j < input_time; ++j)
            {
                if(j % frate == 0)
                {
                    sim.apply_input(i, 500, j);
                    input_fire_cnt++;
                }
            }
        }

        // Simulate with sufficient time (intentionally extra)
        sim.simulate(runtime);
        accumulations += sim.get_metric("accumulate_count");
        fire_cnt += sim.get_metric("fire_count");
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

    double avg_input_fires = static_cast<double>(input_fire_cnt) / static_cast<double>(runs);
    double avg_fires = static_cast<double>(fire_cnt) / static_cast<double>(runs);
    double avg_accum = static_cast<double>(accumulations) / static_cast<double>(runs);

    fmt::print("Average Simulate (s)     : {}\n", avg);
    fmt::print("Median Simulate  (s)     : {}\n", sim_times[sim_times.size()/2].count());
    fmt::print("Input Fires              : {}\n", avg_input_fires);
    fmt::print("Fires                    : {}\n", avg_fires);
    fmt::print("Fires per second         : {:.1f}\n", avg_fires / avg);
    fmt::print("Fires per step           : {:.1f}\n", avg_fires / runtime);
    fmt::print("Accumulations            : {}\n", accumulations);
    fmt::print("Accumulations per second : {:.1f}\n", avg_accum / avg);
    fmt::print("Accumulations per step   : {:.1f}\n", avg_accum / runtime);
    fmt::print("Effective Clock Speed    : {:.4f} KHz\n", (static_cast<double>(runtime) / avg) / (1000) );
}

int main(int argc, char **argv)
{
    int inputs, runs, rt, seed, input_time;

    if(argc < 5)
    {
        fmt::print("Usage: {} inputs n_runs runtime seed (input_time)\n", argv[0]);
        exit(1);
    }

    inputs = atoi(argv[1]);
    runs = atoi(argv[2]);
    rt = atoi(argv[3]);
    seed = atoi(argv[4]);

    if(argc > 5)
    {
        input_time = atoi(argv[5]);
    }
    else
    {
        input_time = rt;
    }

    run_test(inputs, runs, seed, rt, input_time);
    return 0;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
