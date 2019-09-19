#include "network.hpp"
#include "simulator.hpp"
#include <iostream>
#include <vector>
#include <chrono>

using namespace caspian;

void generate_pass(Network *net, int width, int height, int delay = 1)
{
    auto idx = [&width](int r, int c) {
        return r * width + c;
    };

    for(int row = 0; row < height; ++row)
    {
        for(int col = 0; col < width; ++col)
        {
            net->add_neuron(idx(row,col), 1, 0);
            if(col != 0)
            {
                net->add_synapse(idx(row, col-1), idx(row,col), 128, delay);
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

void run_test(int w, int h, int runs, int runtime = 0)
{
    // Create simulator
    Simulator *sim = new Simulator();
    Network *net = new Network(w*h);

    std::vector<std::chrono::duration<double>> sim_times;

    // Generate the pass network
    generate_pass(net, w, h);

    // Configure the simulator with the new network
    auto cfg_start = std::chrono::system_clock::now();
    sim->configure(net);

    auto cfg_end = std::chrono::system_clock::now();

    std::cout << "Width: " << w << " Height: " << h << " Cycles: " << 3*w + 2*h << std::endl;
    std::cout << "Configure: " << (cfg_end - cfg_start).count() / 1000.0 << " us" << std::endl;

    for(int r = 0; r < runs; ++r)
    {
        auto sim_start = std::chrono::system_clock::now();
        // Queue up inputs
        for(int i = 0; i < h; ++i)
        {
            sim->apply_input(i, 500, i);
        }

        // Simulate with sufficient time (intentionally extra)
        int cycles = (runtime == 0) ? 3*w + 2*h : runtime;
        sim->simulate(cycles);
        auto sim_end = std::chrono::system_clock::now();

        std::chrono::duration<double> sim_time = sim_end - sim_start;
        std::cout << "Simulate " << r << ": " << (sim_time).count() << " s" << std::endl;
        sim_times.push_back(sim_time);

        sim->clear_activity();
    }

    std::sort(sim_times.begin(), sim_times.end());

    double avg = 0;
    for(auto const &t : sim_times) avg += t.count();
    avg /= sim_times.size();
    std::cout << "Average Simulate: " << avg << " s" << std::endl;
    std::cout << "Median Simulate: " << sim_times[sim_times.size()/2].count() << " s" << std::endl;

    auto free_start = std::chrono::system_clock::now();
    delete net;
    auto free_netend = std::chrono::system_clock::now();
    delete sim;
    auto free_end = std::chrono::system_clock::now();

    std::cout << "Net Free: "   << (free_netend - free_start).count() / 1000.0 << " us" << std::endl;
    std::cout << "Sim Free: "   << (free_end - free_netend).count()   / 1000.0 << " us" << std::endl;
    std::cout << "Total Free: " << (free_end - free_start).count()    / 1000.0 << " us" << std::endl;
}

int main(int argc, char **argv)
{
    int w = 2000;
    int h = 2000;
    int runs = 3;
    int rt = 0;

    if(argc < 4) std::cout << "Usage: " << argv[0] << " width height n_runs (runtime)" << std::endl;

    if(argc >= 4)
    {
        w = atoi(argv[1]);
        h = atoi(argv[2]);
        runs = atoi(argv[3]);
    }

    if(argc >= 5)
    {
        rt = atoi(argv[4]);
    }

    run_test(w, h, runs, rt);
    return 0;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
