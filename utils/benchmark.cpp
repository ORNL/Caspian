#include "network.hpp"
#include "simulator.hpp"
#include "ucaspian.hpp"
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
            net->add_neuron(idx(row,col), 1, 0);
            if(col != 0)
            {
                net->add_synapse(idx(row, col-1), idx(row,col), 127, delay);
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

void run_test(Backend *sim, int w, int h, int runs, int runtime = 0, int ifires = 1)
{
    // Create simulator
    //Simulator *sim = new Simulator();
    Network *net = new Network(w*h);

    std::vector<std::chrono::duration<double>> sim_times;

    // Generate the pass network
    generate_pass(net, w, h);

    // Configure the simulator with the new network
    auto cfg_start = std::chrono::system_clock::now();
    sim->configure(net);

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
                //sim->apply_input(i, 255, f+i);
                sim->apply_input(i, 255, f*h+i);
            }
        }

        // Simulate with sufficient time (intentionally extra)
        sim->simulate(cycles);
        auto sim_end = std::chrono::system_clock::now();

        std::chrono::duration<double> sim_time = sim_end - sim_start;
        fmt::print("Simulate {:4d}: {} s\n", r, sim_time.count());
        sim_times.push_back(sim_time);

        accumulations += sim->get_metric("accumulate_count");
        fires += sim->get_metric("fire_count");

        for(int i = 0; i < h; ++i)
        {
            fmt::print("Output {}: {}\n", i, sim->get_output_count(i));
            outputs += sim->get_output_count(i);
        }

        sim->clear_activity();
    }

    std::sort(sim_times.begin(), sim_times.end());

    double ttime = 0;
    for(auto const &t : sim_times) ttime += t.count();
    double avg = ttime / sim_times.size();


    auto free_start = std::chrono::system_clock::now();
    delete net;
    auto free_netend = std::chrono::system_clock::now();
    delete sim;
    auto free_end = std::chrono::system_clock::now();
    fmt::print("Average Simulate Time: {} s\n", avg);

    fmt::print("Simulation Stats:\n");
    fmt::print("  > Fires:             {}\n", fires);
    fmt::print("  > Fires/s:           {:.2f}\n", double(fires) / ttime); 
    fmt::print("  > Acumulations:      {}\n", accumulations);
    fmt::print("  > Accum/s:           {:.2f}\n", double(accumulations) / ttime); 
    fmt::print("  > Outputs:           {}\n", outputs);

    fmt::print("Deconstruct Timings:\n");
    fmt::print("  > Network:           {:.2f} us\n", (free_netend - free_start).count() / 1000.0);
    fmt::print("  > Simulator:         {:.2f} us\n", (free_end - free_netend).count() / 1000.0);
    fmt::print("  Total:               {:.2f} us\n", (free_end - free_start).count() / 1000.0);
}

int main(int argc, char **argv)
{
    std::string backend;
    int w = 2000;
    int h = 2000;
    int runs = 3;
    int rt = 0;
    int ifires = 1;

    if(argc < 5)
    {
        fmt::print("Usage: {} backend width height n_runs (runtime) (fires)\n", argv[0]);
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

    Backend *sim = nullptr;

    if(backend == "sim")
    {
        fmt::print("Using Simulator backend\n");
        sim = new Simulator();
    }
    else if(backend == "ucaspian")
    {
        fmt::print("Using uCaspian backend\n");
        sim = new UsbCaspian(false);
    }
#ifdef WITH_VERILATOR
    else if(backend == "verilator")
    {
        fmt::print("Using uCaspian Verilator backend\n");
        sim = new VerilatorCaspian(false);
    }
#endif
    else
    {
        fmt::print("Backend options: sim, ucaspian\n");
        return 0;
    }

    run_test(sim, w, h, runs, rt, ifires);

    return 0;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
