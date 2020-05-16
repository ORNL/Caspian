#include "network.hpp"
#include "simulator.hpp"
#include "ucaspian.hpp"
#include <memory>
#include <iostream>
#include <vector>
#include <chrono>
#include <fmt/format.h>
#include <fmt/ostream.h>

using namespace caspian;

void generate_pass(Network *net, int width, int height, int delay = 0)
{
    auto idx = [width](int r, int c) {
        return r * width + c;
    };

    for(int row = 0; row < height; ++row)
    {
        for(int col = 0; col < width; ++col)
        {
            net->add_neuron(idx(row,col), 1, -1, delay);
            if(col != 0)
            {
                net->add_synapse(idx(row, col-1), idx(row,col), 127, 0);
            }

            if(col == 0)
            {
                net->set_input(idx(row, col), row);
            }
            else if(col == width-1)
            {
                net->set_output(idx(row, col), row);
            }
        }
    }
}

void run_test(Backend *sim, int w, int h, int runs, int runtime = 0, int ifires = 1, int adly = 0)
{
    auto net = std::make_unique<Network>(w*h);
    std::vector<std::chrono::duration<double>> sim_times;

    // Generate the pass network
    generate_pass(net.get(), w, h, adly);

    // Configure the simulator with the new network
    auto cfg_start = std::chrono::system_clock::now();
    sim->configure(net.get());
    sim->track_timing(0);

    auto cfg_end = std::chrono::system_clock::now();

    int cycles = (runtime == 0) ? 3*w + 2*h : runtime;
    fmt::print("Width: {} Height: {} Cycles: {}\n", w, h, cycles);
    fmt::print("Neurons: {} Synapses: {}\n", net->num_neurons(), net->num_synapses());
    fmt::print("Configuration Time: {} us\n", (cfg_end - cfg_start).count() / 1000.0);

    uint64_t accumulations = 0;
    uint64_t fires = 0;
    uint64_t outputs = 0;

    for(int r = 0; r < runs; ++r)
    {
        auto sim_start = std::chrono::system_clock::now();
        // Queue up inputs
        for(int f = 0; f < ifires; ++f)
        {
            for(int i = 0; i < h; ++i)
            {
                sim->apply_input(i, 255, f*h+i);
            }
        }

        // Simulate with sufficient time
        sim->simulate(cycles);
        auto sim_end = std::chrono::system_clock::now();

        std::chrono::duration<double> sim_time = sim_end - sim_start;
        fmt::print("Simulate {:4d}: {} s\n", r, sim_time.count());
        sim_times.push_back(sim_time);

        accumulations += sim->get_metric("accumulate_count");
        fires += sim->get_metric("fire_count");

        for(int i = 0; i < h; ++i)
        {
            fmt::print("Output {} ({}):", i, sim->get_output_count(i));
            auto outs = sim->get_output_values(i);
            for(auto o : outs) fmt::print(" {}", o);
            fmt::print("\n");
            outputs += sim->get_output_count(i);
        }

        sim->clear_activity();
    }

    std::sort(sim_times.begin(), sim_times.end());

    double ttime = 0;
    for(auto const &t : sim_times) ttime += t.count();
    double avg = ttime / sim_times.size();

    fmt::print("Average Simulate Time: {}\n", avg);

    fmt::print("Simulation Stats:\n");
    fmt::print("  > Fires:             {}\n", fires);
    fmt::print("  > Fires/s:           {:.2f}\n", double(fires) / ttime); 
    fmt::print("  > Acumulations:      {}\n", accumulations);
    fmt::print("  > Accum/s:           {:.2f}\n", double(accumulations) / ttime); 
    fmt::print("  > Outputs:           {}\n", outputs);
}

int main(int argc, char **argv)
{
    std::string backend;
    int w = 250;
    int h = 1;
    int runs = 1;
    int rt = 0;
    int ifires = 1;
    int dly = 0;

    if(argc < 5)
    {
        fmt::print("Usage: {} backend width height n_runs (runtime) (fires) (delay)\n", argv[0]);
        return -1;
    }

    if(argc >= 5)
    {
        backend = argv[1];
        w = atoi(argv[2]);
        h = atoi(argv[3]);
        runs = atoi(argv[4]);
    }

    if(argc >= 6)
    {
        rt = atoi(argv[5]);
    }

    if(argc >= 7)
    {
        ifires = atoi(argv[6]);
    }

    if(argc >= 8)
    {
        dly = atoi(argv[7]);
    }

    if(dly > 15)
    {
        fmt::print("Delay may not be greater than 15! Given {}\n", dly);
        return -1;
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
        fmt::print("Using uCaspian Verilator backend - debug => pass.fst\n");
        sim = std::make_unique<VerilatorCaspian>(false, "pass.fst");
    }
#endif
    else
    {
        fmt::print("Backend options: sim, ucaspian\n");
        return 0;
    }

    try {
        run_test(sim.get(), w, h, runs, rt, ifires, dly);
    }
    catch(...)
    {
        fmt::print("There was an error completing the test.\n");
    }

    return 0;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
