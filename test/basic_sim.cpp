#include "doctest/doctest.h"
#include "network.hpp"
#include "simulator.hpp"
#include <iostream>
#include <vector>
#include <set>
#include <fstream>
#include <algorithm>

using namespace caspian;

static void generate_pass(Network *net, int width, int height, int delay = 1)
{

    for(int i = 0; i < height; i++)
    {
        int row = i;

        for(int col = 0; col < width; col++)
        {
            net->add_neuron(row * width + col, 1);
            if(col != 0)
            {
                uint32_t idx = row * width + col;
                net->add_synapse(idx-1, idx, 128, delay);
            }

            if(col == 0)
            {
                net->set_input(row * width, i);
            }
            else if(col == width-1)
            {
                net->set_output(row * width + col, i);
            }
        }
    }
}

static void generate_simple(Network *net, int n_thresh, int s_weight, int s_delay, int n_leak = -1, int n_delay = 0)
{
    // input neuron
    net->add_neuron(0, 0, n_leak, n_delay);
    net->set_input(0, 0);

    // neuron under test
    net->add_neuron(1, n_thresh, n_leak, n_delay);
    net->set_output(1, 0);

    // synapse under test
    net->add_synapse(0, 1, s_weight, s_delay);
}

TEST_CASE("Simulation of a Straight Pass Network")
{
    // Configure the test cases
    const std::vector<int> ws = {2, 5, 10, 50, 100};
    const std::vector<int> hs = {2, 5, 10, 15, 20, 25, 50, 100};
    int w, h;

    // Create simulator
    Simulator *sim = new Simulator();

    // enumerate the possible width/height combinations
    for(size_t a = 0; a < ws.size(); ++a)
    {
        w = ws[a];
        for(size_t b = 0; b < hs.size(); ++b)
        {
            h = hs[b];

            Network *net = new Network(w * h);

            // Generate the pass network
            generate_pass(net, w, h);

            // Check network
            CHECK(net->size() == w * h);

            // Configure the simulator with the new network
            sim->configure(net);

            // monitor outputs
            for(uint32_t i = 0; i < net->num_outputs(); ++i)
            {
                sim->track_timing(i);
            }

            // check time
            CHECK(net->get_time() == 0);
            CHECK(sim->get_time() == 0);

            // Queue up inputs
            for(int i = 0; i < h; ++i)
            {
                sim->apply_input(i, 500, i);
            }

            // Simulate with sufficient time (intentionally extra)
            uint64_t sim_time = 3*w + 2*h;
            sim->simulate(sim_time);

            // Check outputs
            for(int i = 0; i < h; ++i)
            {
                std::vector<uint32_t> times = sim->get_output_values(i);
                CHECK(sim->get_output_count(i) == 1);
                REQUIRE(times.size() == 1);
                CHECK(times[0] == 2*(w-1)+i);
            }

            // Check metrics -- for this test, accumulate and fire will be the same
            CHECK(sim->get_metric("accumulate_count") == w*h);
            CHECK(sim->get_metric("fire_count") == w*h);
            CHECK(sim->get_metric("total_timesteps") == sim_time);

            // Check metrics reset
            CHECK(sim->get_metric("accumulate_count") == 0);
            CHECK(sim->get_metric("fire_count") == 0);
            CHECK(sim->get_metric("total_timesteps") == 0);

            sim->configure(nullptr);

            delete net;
        }
    }

    delete sim;
}

TEST_CASE("Check total_timesteps metric after simulation")
{
    Simulator *sim = new Simulator();
    Network *net = new Network();

    // Generate the pass network
    generate_pass(net, 5, 5);

    sim->configure(net);
            
    sim->simulate(100);
    sim->simulate(100);
    sim->simulate(100);
    sim->simulate(100);
    CHECK(sim->get_metric("total_timesteps") == 400);
    CHECK(sim->get_metric("total_timesteps") == 0);

    sim->simulate(100);
    sim->clear_activity();
    sim->simulate(100);
    CHECK(sim->get_metric("total_timesteps") == 200);
    CHECK(sim->get_metric("total_timesteps") == 0);

    sim->configure(nullptr);

    delete net;
    delete sim;
}

TEST_CASE("Check correctness of threshold and weight comparison in simulation")
{
    Simulator *sim = new Simulator();
    // format: threshold, weight
    const std::vector<std::pair<int, int>> test_cases = { {0, 0}, {0, 1}, {1, 0}, {1, 1}, {1, 2} };
    const std::vector<bool> does_fire                 = { false,  true,   false,  false,  true   };

    for(size_t i = 0; i < test_cases.size(); ++i)
    {
        // Create one tile, simple network
        Network *net = new Network(25);

        // generate test network
        generate_simple(net, test_cases[i].first, test_cases[i].second, 0);

        // Configure simulator
        sim->configure(net);

        // Queue inputs
        sim->apply_input(0, 100, 0);

        // Simulate
        sim->simulate(10);

        // Check outputs and verify
        CHECK( (sim->get_output_count(0) == 1) == does_fire[i] );

        sim->configure(nullptr);
        delete net;
    }

    delete sim;
}

TEST_CASE("Synapses correctly delay fires in simulation")
{
    const uint8_t max_delay = constants::MAX_DELAY;
    Network net(25);
    Simulator sim;
    uint32_t a = 0, b = 1;

    // construct network w/o delay initially
    net.add_neuron(a, 1);
    net.add_neuron(b, 1);
    net.add_synapse(a, b, 100, 0);
    net.set_input(a, 0);
    net.set_output(b, 0);

    for(uint8_t delay = 0; delay < max_delay; ++delay)
    {
        // Update synapse delay by overwriting synapse
        net.remove_synapse(a, b);
        net.add_synapse(a, b, 100, delay);

        // Configure the device
        sim.configure(&net);
        sim.track_timing(0);

        // Queue inputs
        for(int i = 0; i < 10; ++i) sim.apply_input(0, 200, i);
        
        sim.simulate(max_delay+11);

        // Get outputs
        std::vector<uint32_t> out = sim.get_output_values(0);
        REQUIRE(out.size() == 10);
        for(int i = 0; i < 10; ++i)
            CHECK(out.at(i) == 1+i+delay);

        sim.reset();
    }
}

TEST_CASE("Check functionality of axon and synaptic delay used together")
{
    Simulator *sim = new Simulator();
    // format: syn delay, axon delay
    const std::vector<std::pair<int, int>> test_cases = { {0, 0}, {1, 0}, {0, 1}, {1, 1}, {15, 0}, {0, 15}, {15, 15} };
    const std::vector<int> fire_time                  = {      1,      2,      2,      3,      16,      16,       31 };

    for(size_t i = 0; i < test_cases.size(); ++i)
    {
        // Create one tile, simple network
        Network *net = new Network(25);

        // generate test network
        generate_simple(net, 10, 100, test_cases[i].first, 0, test_cases[i].second);

        // Check network
        CHECK(net->max_syn_delay >= test_cases[i].first);
        CHECK(net->max_axon_delay >= test_cases[i].second);

        // Configure simulator
        sim->configure(net);

        // Queue inputs
        sim->apply_input(0, 127, 0);

        // Simulate
        sim->simulate(50);

        // Check outputs and verify
        CHECK(sim->get_last_output_time(0) == fire_time[i]);

        sim->configure(nullptr);
        delete net;
    }

    delete sim;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
