#include "network.hpp"
#include "simulator.hpp"
#include "ucaspian.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <fmt/format.h>
#include <fmt/ostream.h>

using namespace caspian;

void run_test(Backend *sim, int inputs, int runs, int seed, int runtime, int input_time, bool use_delay)
{
    auto rand_start = std::chrono::system_clock::now();

    const int max_weight = 127;
    const int threshold = 100;
    const int max_delay = 15;
    std::mt19937 gen{seed};
    std::normal_distribution<> nd{0,1};
    //std::uniform_int_distribution<> ud(0, max_delay);

    int n_neurons = inputs;
    uint64_t accumulations = 0;
    uint64_t fire_cnt = 0;
    uint64_t input_fire_cnt = 0;

    std::vector<std::chrono::duration<double>> sim_times;
    std::vector<long long> output_counts;

    Network net(n_neurons);

    // Generate the network
    for(int i = 0; i < inputs; ++i)
    {
        int dd = std::round(nd(gen) * (max_delay/2));
        int dly = std::max(0, std::min(max_delay, dd));

        if(!use_delay) dly = 0;

        net.add_neuron(i, threshold, -1, dly);
        net.set_input(i, i);
        net.set_output(i, i);
    }

    for(int i = 0; i < inputs; ++i)
    {
        for(int j = 0; j < inputs; ++j)
        {
            if(j == i) continue;

            int wd = std::round(nd(gen) * (threshold/2));
            int w = std::max(-max_weight, std::min(max_weight, wd));

            //int dd = std::round(nd(gen) * 15);
            //int dly = std::max(0, std::min(max_delay, dd));
            //int dly = ud(gen);
            int dly = 0;

            net.add_synapse(i, j, w, dly);
        }
    }
    

    // Configure the simulator with the new network
    auto cfg_start = std::chrono::system_clock::now();

    sim->configure(&net);

    for(int i = 0; i < inputs; i++)
    {
        sim->track_timing(i);
    }

    auto cfg_end = std::chrono::system_clock::now();

    std::chrono::duration<double, std::micro> rnd_duration = (cfg_start - rand_start);
    std::chrono::duration<double, std::micro> cfg_duration = (cfg_end - cfg_start);

    fmt::print("Seed: {} | Neurons: {} Synapses: {} | Cycles: {} | Input Duration: {}\n", 
            seed, net.num_neurons(), net.num_synapses(), runtime, input_time);
    fmt::print("Random Net: {} us\n", rnd_duration.count()); 
    fmt::print("Configure : {} us\n", cfg_duration.count());

    for(int r = 0; r < runs; ++r)
    {
        std::uniform_int_distribution<> dis(0, 100);

        auto sim_start = std::chrono::system_clock::now();

        // Queue up inputs
        for(int i = 0; i < inputs; ++i)
        {
            int frate = dis(gen);

            if(frate == 0) continue;

            for(int j = 0; j < input_time; ++j)
            {
                if(j % frate == 0)
                {
                    sim->apply_input(i, 255, j);
                    input_fire_cnt++;
                }
            }
        }

        // Simulate with specified time
        sim->simulate(runtime);
        accumulations += sim->get_metric("accumulate_count");
        fire_cnt += sim->get_metric("fire_count");

        int cnts = 0;
        for(int i = 0; i < inputs; ++i) cnts += sim->get_output_count(i);
        output_counts.push_back(cnts);

        //accumulations += sim->get_metric_uint("accumulate_count");
        //fire_cnt += sim->get_metric_uint("fire_count");
        auto sim_end = std::chrono::system_clock::now();

        std::chrono::duration<double> sim_time = sim_end - sim_start;
        fmt::print("Simulate {:4d}: {} s\n", r, sim_time.count());
        sim_times.push_back(sim_time);

        //
        for(int i = 0; i < inputs; i++)
        {
            fmt::print("{:3d} ({:3d}):", i, sim->get_output_count(i));
            auto vec = sim->get_output_values(i);
            for(auto v : vec) fmt::print(" {}", v);
            fmt::print("\n");
        }
        //

        sim->clear_activity();
    }

    std::sort(sim_times.begin(), sim_times.end());

    double avg = 0;
    for(auto const &t : sim_times) avg += t.count();
    avg /= sim_times.size();

    long long ocnts = std::accumulate(output_counts.begin(), output_counts.end(), 0);

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
    fmt::print("Output Counts            : {:12d}\n", ocnts);
}

int main(int argc, char **argv)
{
    std::string backend;
    int inputs, runs, rt, seed, input_time;
    bool use_delay;

    if(argc < 6)
    {
        fmt::print("Usage: {} backend inputs n_runs runtime seed (delay: Y|N) (input_time)\n", argv[0]);
        exit(1);
    }

    backend = argv[1];
    inputs = atoi(argv[2]);
    runs = atoi(argv[3]);
    rt = atoi(argv[4]);
    seed = atoi(argv[5]);
    use_delay = false;

    if(argc > 6)
    {
        if(argv[6][0] == 'Y')
        {
            use_delay = true;
            fmt::print("Using axonal delay\n");
        }
    }

    if(argc > 7)
    {
        input_time = atoi(argv[7]);
    }
    else
    {
        input_time = rt;
    }

    Backend *sim = nullptr;

    if(backend == "sim")
    {
        fmt::print("Using Simulator backend\n");
        sim = new Simulator();
    }
    else if(backend == "debug")
    {
        fmt::print("Using Simulator backend\n");
        sim = new Simulator(true);
    }
    else if(backend == "ucaspian")
    {
        fmt::print("Using uCaspian backend\n");
        sim = new UsbCaspian(false, "/dev/ttyUSB0");
    }
#ifdef WITH_VERILATOR
    else if(backend == "verilator")
    {
        fmt::print("Using uCaspian Verilator backend\n");
        sim = new VerilatorCaspian(false);
        //sim = new VerilatorCaspian(false, "a2a.fst");
    }
    else if(backend == "verilator-log")
    {
        fmt::print("Using uCaspian Verilator backend - debug => a2a.fst\n");
        sim = new VerilatorCaspian(false, "a2a.fst");
    }
#endif
    else
    {
        fmt::print("Backend options: sim, ucaspian\n");
        return 0;
    }

    run_test(sim, inputs, runs, seed, rt, input_time, use_delay);

    delete sim;
    return 0;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
