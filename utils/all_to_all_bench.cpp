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

void run_test(Backend *sim, int inputs, int runs, int seed, int runtime, int input_time, bool use_delay, bool print_outputs)
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
    uint64_t input_fire_cnt = 0;
    uint64_t active_cycles = 0;

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

            // NOTE: No synaptic delay for now!
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
        active_cycles += sim->get_metric("active_clock_cycles");

        int cnts = 0;
        for(int i = 0; i < inputs; ++i) cnts += sim->get_output_count(i);
        output_counts.push_back(cnts);

        auto sim_end = std::chrono::system_clock::now();

        std::chrono::duration<double> sim_time = sim_end - sim_start;
        fmt::print("Simulate {:4d}: {} s\n", r, sim_time.count());
        sim_times.push_back(sim_time);

        if(print_outputs)
        {
            for(int i = 0; i < inputs; i++)
            {
                fmt::print("{:3d} ({:3d}):", i, sim->get_output_count(i));
                auto vec = sim->get_output_values(i);
                for(auto v : vec) fmt::print(" {}", v);
                fmt::print("\n");
            }
        }

        sim->clear_activity();
    }

    std::sort(sim_times.begin(), sim_times.end());

    double avg = 0;
    for(auto const &t : sim_times) avg += t.count();
    avg /= sim_times.size();

    long long ocnts = std::accumulate(output_counts.begin(), output_counts.end(), 0);

    double avg_input_fires = static_cast<double>(input_fire_cnt) / static_cast<double>(runs);
    double avg_accum = static_cast<double>(accumulations) / static_cast<double>(runs);

    fmt::print("\n");
    fmt::print("---[Metrics]------------------------\n");
    fmt::print("Average Simulate (s)     : {:9.7f}\n", avg);
    fmt::print("Median Simulate  (s)     : {:9.7f}\n", sim_times[sim_times.size()/2].count());
    fmt::print("Input Spikes             : {}\n", avg_input_fires);
    fmt::print("Output Spikes            : {}\n", ocnts);
    fmt::print("Accumulations            : {}\n", accumulations);
    fmt::print("Accumulations/second     : {:.1f}\n", avg_accum / avg);
    fmt::print("Accumulations/step       : {:.1f}\n", avg_accum / runtime);
    fmt::print("Effective Speed (KHz)    : {:.4f}\n", (static_cast<double>(runtime) / avg) / (1000) );

    if(active_cycles != 0)
    {
        // This is dependent on the actual clock speed of the dev board.
        // Here, we assume 25 MHz as the standard on the uCaspian rev0 board.
        double adj_time = (static_cast<double>(active_cycles) / 25000000.0) / static_cast<double>(runs);
        fmt::print("---[FPGA Metrics]-------------------\n");
        fmt::print("Active Clock Cycles      : {}\n", active_cycles);
        fmt::print("Adj Runtime (s)          : {:9.7f}\n", adj_time);
        fmt::print("Adj Accumulations/second : {:.1f}\n", avg_accum / adj_time);
        fmt::print("Adj Effective Speed (KHz): {:.4f}\n", (runtime / adj_time) / (1000) );
    }
}

int main(int argc, char **argv)
{
    if(argc < 6)
    {
        fmt::print("Usage: {} backend inputs n_runs runtime seed (delay: Y|N) (print_outputs: Y|N) (input_time)\n", argv[0]);
        exit(1);
    }

    std::string backend = argv[1];
    int inputs = atoi(argv[2]);
    int runs = atoi(argv[3]);
    int rt = atoi(argv[4]);
    int seed = atoi(argv[5]);
    bool use_delay = false;
    bool print_outputs = false;
    int input_time = rt;

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
        if(argv[7][0] == 'Y')
        {
            print_outputs = true;
        }
    }

    if(argc > 8)
    {
        input_time = atoi(argv[8]);
    }

    std::unique_ptr<Backend> sim;

    if(backend == "sim")
    {
        fmt::print("Using Simulator backend\n");
        sim = std::make_unique<Simulator>();
    }
    else if(backend == "debug")
    {
        fmt::print("Using Simulator backend\n");
        sim = std::make_unique<Simulator>(true);
    }
    else if(backend == "ucaspian")
    {
        fmt::print("Using uCaspian backend\n");
        sim = std::make_unique<UsbCaspian>(false);
    }
#ifdef WITH_VERILATOR
    else if(backend == "verilator")
    {
        fmt::print("Using uCaspian Verilator backend\n");
        sim = std::make_unique<VerilatorCaspian>(false);
    }
    else if(backend == "verilator-log")
    {
        fmt::print("Using uCaspian Verilator backend - debug => a2a.fst\n");
        sim = std::make_unique<VerilatorCaspian>(false, "a2a.fst");
    }
#endif
    else
    {
#ifdef WITH_VERILATOR
        fmt::print("Backend options: sim, debug, ucaspian, verilator, verilator-log\n");
#else
        fmt::print("Backend options: sim, debug, ucaspian\n");
#endif
        return 0;
    }

    run_test(sim.get(), inputs, runs, seed, rt, input_time, use_delay, print_outputs);

    return 0;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
