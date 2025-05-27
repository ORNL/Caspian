#include <vector>

#include "doctest/doctest.h"
#include "network.hpp"
#include "simulator.hpp"

using namespace caspian;

static void generate_pass(Network *net, int width, int height, int delay)
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

TEST_CASE("Simulation of a Straight Pass Network")
{
    const int h = 2;
    const int nt = 25;
    const int stime = 42; 

    // Create simulator
    Simulator sim;

    std::vector<Network*> networks;

    for(int i = 2; i < 2+nt; i++)
    {
        Network *net = new Network();
        generate_pass(net, i, h, 1);
        networks.push_back(net);
    }

    // Configure the simulator with the new network
    REQUIRE(sim.configure_multi(networks));

    // monitor outputs
    for(uint32_t i = 0; i < networks[0]->num_outputs(); ++i)
    {
        sim.track_timing(i);
    }

    // check time
    CHECK(sim.get_time() == 0);

    // Queue up inputs
    for(int i = 0; i < h; ++i)
    {
        sim.apply_input(i, 500, i);
    }

    // Simulate with set time
    sim.simulate(stime);

    // Check outputs
    for(int i = 0; i < nt; ++i)
    {
        if(i > 18)
        {
            CHECK(sim.get_output_count(1, i) == 0);

            if(i > 19)
            {
                CHECK(sim.get_output_count(0, i) == 0);
            }
            else
            {
                CHECK(sim.get_output_count(0, i) == 1);
                CHECK(sim.get_last_output_time(0, i) == 2*(i+1)+1);
            }
        }
        else
        {
            CHECK(sim.get_output_count(0, i) == 1);
            CHECK(sim.get_output_count(1, i) == 1);
            CHECK(sim.get_last_output_time(0, i) == 2*(i+1)+1);
            CHECK(sim.get_last_output_time(1, i) == 2*(i+1)+2);
        }
        CHECK(networks[i]->get_time() == stime);
    }

    // Clean up 
    for(size_t i = 0; i < networks.size(); i++)
    {
        delete networks[i];
    }
    networks.clear();
    
}
