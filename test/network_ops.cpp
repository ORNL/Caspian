#include <vector>
#include <algorithm>
#include "doctest/doctest.h"
#include "network.hpp"
#include "simulator.hpp"

using namespace caspian;

TEST_CASE("Sizes")
{
    printf("Size of:\n");
    printf(" Neuron: %lu\n", sizeof(Neuron));
    printf(" Synapse: %lu\n", sizeof(Synapse));
    printf(" Network: %lu\n", sizeof(Network));
    printf(" Event: %lu\n", sizeof(FireEvent));
    printf(" Simulator: %lu\n", sizeof(Simulator)); 
}

TEST_CASE("Neurons may be added, retrieved, and deleted")
{
    Network net;
    uint32_t c = 0;

    // Add a neuron
    net.add_neuron(c, 1);
    REQUIRE(net.is_neuron(c));

    // Check its initial values
    Neuron &n = net.get_neuron(c);
    CHECK(n.threshold == 1);
    CHECK(n.leak == -1);
    CHECK(n.id == c);

    auto va = net.get_neuron_list();
    REQUIRE(va.size() == 1);
    CHECK(va[0] == c);

    // Remove the neuron
    net.remove_neuron(c);
    REQUIRE_FALSE(net.is_neuron(c));
    
    auto vb = net.get_neuron_list();
    REQUIRE(vb.size() == 0);
}

TEST_CASE("Network metrics are correctly reported")
{
    Network net(5);
    uint32_t ca = 0, cb = 1, cc = 2, cd = 3;

    net.add_neuron(ca, 2);
    REQUIRE(net.get_metric("neuron_count") == 1);
    REQUIRE(net.num_neurons() == 1);

    net.add_neuron(cb, 2);
    REQUIRE(net.get_metric("neuron_count") == 2);
    REQUIRE(net.num_neurons() == 2);

    net.add_neuron(cc, 1);
    REQUIRE(net.get_metric("neuron_count") == 3);
    REQUIRE(net.num_neurons() == 3);

    net.add_neuron(cd, 2);
    REQUIRE(net.get_metric("neuron_count") == 4);
    REQUIRE(net.num_neurons() == 4);

    net.remove_neuron(cd);
    REQUIRE(net.get_metric("neuron_count") == 3);
    REQUIRE(net.num_neurons() == 3);

    REQUIRE(net.get_metric("synapse_count") == 0);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 0);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 0);
    REQUIRE(net.num_synapses() == 0);

    net.add_synapse(ca, cb, 100, 0);
    REQUIRE(net.get_metric("synapse_count") == 1);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 0);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 1);
    REQUIRE(net.num_synapses() == 1);

    net.add_synapse(cb, ca, -100, 0);
    REQUIRE(net.get_metric("synapse_count") == 2);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 1);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 1);
    REQUIRE(net.num_synapses() == 2);

    net.add_synapse(cc, cb, 1, 15);
    REQUIRE(net.get_metric("synapse_count") == 3);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 1);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 2);
    REQUIRE(net.num_synapses() == 3);

    net.add_synapse(cb, cc, -1, 15);
    REQUIRE(net.get_metric("synapse_count") == 4);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 2);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 2);
    REQUIRE(net.num_synapses() == 4);

    net.remove_synapse(ca, cb);
    REQUIRE(net.get_metric("synapse_count") == 3);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 2);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 1);
    REQUIRE(net.num_synapses() == 3);

    net.remove_synapse(cb, ca);
    REQUIRE(net.get_metric("synapse_count") == 2);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 1);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 1);
    REQUIRE(net.num_synapses() == 2);

    net.remove_synapse(cc, cb);
    REQUIRE(net.get_metric("synapse_count") == 1);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 1);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 0);
    REQUIRE(net.num_synapses() == 1);

    net.remove_synapse(cb, cc);
    REQUIRE(net.get_metric("synapse_count") == 0);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 0);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 0);
    REQUIRE(net.num_synapses() == 0);

    net.add_synapse(ca, cb, 100, 0);
    net.add_synapse(cb, ca, -100, 0);
    net.add_synapse(cc, cb, 1, 15);
    net.add_synapse(cb, cc, -1, 15);
    REQUIRE(net.get_metric("synapse_count") == 4);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 2);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 2);
    REQUIRE(net.num_synapses() == 4);

    // purge elements should result in everything getting removed
    net.purge_elements();
    REQUIRE(net.size() == 0);
    REQUIRE(net.num_synapses() == 0);
    REQUIRE(net.get_metric("neuron_count") == 0);
    REQUIRE(net.get_metric("synapse_count") == 0);
    REQUIRE(net.get_metric("inhibitory_synapse_count") == 0);
    REQUIRE(net.get_metric("excitatory_synapse_count") == 0);
    REQUIRE(net.num_neurons() == 0);
    REQUIRE(net.num_synapses() == 0);
}

TEST_CASE("Synapses may be added, retrieved, and deleted")
{
    Network net(5);
    uint32_t a = 0, b = 1;

    // Add the neurons to connect up
    net.add_neuron(a, 1);
    net.add_neuron(b, 2);
    CHECK(net.is_neuron(a));
    CHECK(net.is_neuron(b));

    // Add a synapse
    net.add_synapse(a, b, 2, 1);
    CHECK_FALSE(net.is_synapse(b, a));
    CHECK(net.is_synapse(a, b));
    CHECK(net.num_synapses() == 1);

    // Get synapse
    Synapse &s = net.get_synapse(a, b);
    CHECK(s.weight == 2);
    CHECK(s.delay == 1);

    // Check neurons
    Neuron &na = net.get_neuron(a);
    Neuron &nb = net.get_neuron(b);

    REQUIRE(na.outputs.size() == 1);
    CHECK(na.outputs[0].first == & (net.get_neuron(b)));
    CHECK(na.outputs[0].second == & (net.get_synapse(a, b)));
    CHECK(nb.synapses.find(a) != nb.synapses.end());

    // Remove Synapse
    net.remove_synapse(a, b);
    REQUIRE_FALSE(net.is_synapse(a, b));
    CHECK(net.num_synapses() == 0);

    // Check Neurons
    CHECK(na.outputs.size() == 0);
    CHECK(nb.synapses.find(a) == nb.synapses.end());
}

TEST_CASE("Networks can be copy constructed")
{
    Network *net = new Network(10);
    uint32_t a = 0, b = 1, c = 4;

    net->add_neuron(a, 1);
    net->add_neuron(b, 2);
    net->add_neuron(c, 3);

    net->add_synapse(a, b, 10, 1);
    net->add_synapse(a, c, 20, 1);
    net->add_synapse(b, a, 99, 1);
    net->add_synapse(b, c, 88, 1);
    net->add_synapse(c, a,  1, 2); 

    // copy construct
    Network cnet(*net);

    // Free the original network
    delete net;

    // Check stats
    REQUIRE(cnet.get_time() == 0);
    REQUIRE(cnet.num_synapses() == 5);
    REQUIRE(cnet.num_neurons() == 3);

    // everything _should_ exist
    REQUIRE(cnet.is_neuron(a));
    REQUIRE(cnet.is_neuron(b));
    REQUIRE(cnet.is_neuron(c));
    REQUIRE(cnet.is_synapse(a, b));
    REQUIRE(cnet.is_synapse(a, c));
    REQUIRE(cnet.is_synapse(b, a));
    REQUIRE(cnet.is_synapse(b, c));
    REQUIRE(cnet.is_synapse(c, a));

    // Check all of the neurons
    Neuron &na = cnet.get_neuron(a);
    CHECK(na.threshold == 1);
    CHECK(na.synapses.size() == 2);
    CHECK(na.outputs.size() == 2);

    Neuron &nb = cnet.get_neuron(b);
    CHECK(nb.threshold == 2);
    CHECK(nb.synapses.size() == 1);
    CHECK(nb.outputs.size() == 2);

    Neuron &nc = cnet.get_neuron(c);
    CHECK(nc.threshold == 3);
    CHECK(nc.synapses.size() == 2);
    CHECK(nc.outputs.size() == 1);
    
    // spot check syanpse
    Synapse &s = cnet.get_synapse(b, c);
    CHECK(s.weight == 88);
    CHECK(s.delay == 1);
}

TEST_CASE("Networks can be pruned of useless neurons")
{
    Network net(10);

    // add neurons (0-7)
    for(uint32_t i = 0; i < 8; ++i)
        net.add_neuron(i, 100);

    // set up inputs/outputs -- required for prune
    net.set_input(0, 0);
    net.set_output(3, 0);

    // verify all neurons exist
    for(uint32_t i = 0; i < 8; ++i)
        CHECK(net.is_neuron(i));

    // add connections
    net.add_synapse(0, 1, 100, 0);
    net.add_synapse(0, 2, 100, 0);
    net.add_synapse(0, 3, 100, 0);
    net.add_synapse(1, 3, 100, 0);
    net.add_synapse(2, 3, 100, 0);
    net.add_synapse(3, 1, 100, 0);
    net.add_synapse(3, 4, 100, 0);
    net.add_synapse(3, 5, 100, 0);
    net.add_synapse(5, 4, 100, 0);
    net.add_synapse(4, 6, 100, 0);
    
    // function under test -- prune()
    net.prune();

    // check remaining neurons
    CHECK(net.is_neuron(0));
    CHECK(net.is_neuron(1));
    CHECK(net.is_neuron(2));
    CHECK(net.is_neuron(3));
    CHECK_FALSE(net.is_neuron(4));
    CHECK_FALSE(net.is_neuron(5));
    CHECK_FALSE(net.is_neuron(6));
    CHECK_FALSE(net.is_neuron(7));
    CHECK_FALSE(net.is_neuron(8));

    auto v = net.get_neuron_list();

    CHECK(std::find(v.begin(), v.end(), 0) != v.end());
    CHECK(std::find(v.begin(), v.end(), 1) != v.end());
    CHECK(std::find(v.begin(), v.end(), 2) != v.end());
    CHECK(std::find(v.begin(), v.end(), 3) != v.end());

    CHECK(std::find(v.begin(), v.end(), 4) == v.end());
    CHECK(std::find(v.begin(), v.end(), 5) == v.end());
    CHECK(std::find(v.begin(), v.end(), 6) == v.end());
    CHECK(std::find(v.begin(), v.end(), 7) == v.end());
    CHECK(std::find(v.begin(), v.end(), 8) == v.end());

    // check for remaining connections
    CHECK(net.is_synapse(0, 1));
    CHECK(net.is_synapse(0, 2));
    CHECK(net.is_synapse(0, 3));
    CHECK(net.is_synapse(1, 3));
    CHECK(net.is_synapse(2, 3));
    CHECK(net.is_synapse(3, 1));
    CHECK_FALSE(net.is_synapse(3, 4));
    CHECK_FALSE(net.is_synapse(3, 5));
    CHECK_FALSE(net.is_synapse(5, 4));
    CHECK_FALSE(net.is_synapse(4, 6));

    auto sv = net.get_synapse_list();

    auto check_syn = [&](uint32_t from, uint32_t to, bool exists) {
        auto p = std::make_pair(from, to);
        
        if(exists)
            CHECK( std::find(sv.begin(), sv.end(), p) != sv.end() );
        else
            CHECK( std::find(sv.begin(), sv.end(), p) == sv.end() );
    };

    check_syn(0,1,true);
    check_syn(0,2,true);
    check_syn(0,3,true);
    check_syn(1,3,true);
    check_syn(2,3,true);
    check_syn(3,1,true);

    check_syn(3,4,false);
    check_syn(3,5,false);
    check_syn(5,4,false);
    check_syn(4,6,false);
}

TEST_CASE("Networks can be pruned of unused i/o neurons")
{
    Network net(10);

    // add neurons (0-7)
    for(uint32_t i = 0; i < 8; ++i)
        net.add_neuron(i, 100);

    // set up inputs/outputs -- required for prune
    net.set_input(0, 0);
    net.set_input(5, 1);
    net.set_output(3, 0);
    net.set_output(4, 1);
    net.set_output(6, 2);

    // verify all neurons exist
    for(uint32_t i = 0; i < 8; ++i)
        CHECK(net.is_neuron(i));

    net.add_synapse(0, 1, 100, 0);
    net.add_synapse(1, 2, 100, 0);
    net.add_synapse(2, 3, 100, 0);
    net.add_synapse(4, 6, 100, 0);
    net.add_synapse(6, 4, 100, 0);

    net.prune(false);

    CHECK(net.is_neuron(0));
    CHECK(net.is_neuron(1));
    CHECK(net.is_neuron(2));
    CHECK(net.is_neuron(3));
    CHECK(net.is_neuron(4));
    CHECK(net.is_neuron(5));
    CHECK(net.is_neuron(6));
    CHECK_FALSE(net.is_neuron(7));

    CHECK(net.is_synapse(0, 1));
    CHECK(net.is_synapse(1, 2));
    CHECK(net.is_synapse(2, 3));
    CHECK(net.is_synapse(4, 6));
    CHECK(net.is_synapse(6, 4));

    net.prune(true);

    CHECK(net.is_neuron(0));
    CHECK(net.is_neuron(1));
    CHECK(net.is_neuron(2));
    CHECK(net.is_neuron(3));
    CHECK_FALSE(net.is_neuron(4));
    CHECK_FALSE(net.is_neuron(5));
    CHECK_FALSE(net.is_neuron(6));
    CHECK_FALSE(net.is_neuron(7));

    CHECK(net.is_synapse(0, 1));
    CHECK(net.is_synapse(1, 2));
    CHECK(net.is_synapse(2, 3));
    CHECK_FALSE(net.is_synapse(4, 6));
    CHECK_FALSE(net.is_synapse(6, 4));
}


TEST_CASE("Network Serialization")
{
    Network net(20);
    
    // add neurons
    for(uint32_t i = 0; i < 20; ++i)
        net.add_neuron(i, 100 + i);

    // add synapses
    for(uint32_t f = 0; f < 20; ++f)
        for(uint32_t t = 0; t < 20; ++t)
            if(f != t)
                net.add_synapse(f, t, 25 + 20 * f + t, f/2);

    // add inputs
    for(uint32_t i = 0; i < 4; ++i)
        net.set_input(i, i);

    // add outputs
    for(uint32_t i = 0; i < 3; ++i)
        net.set_output(17+i, i);

    // serialize network
    std::string str = net.to_str();

    // create new network from serialization
    Network snet;
    snet.from_str(str);

    REQUIRE(snet.size() == net.size());

    auto nl = snet.get_neuron_list();
    auto sl = snet.get_synapse_list();

    REQUIRE(nl.size() == net.num_neurons());
    REQUIRE(sl.size() == net.num_synapses());

    // check networks match
    for(auto elm : net)
    {
        REQUIRE(snet.is_neuron(elm.first));

        Neuron &n = snet.get_neuron(elm.first);

        CHECK(n.input_id == elm.second->input_id);
        CHECK(n.output_id == elm.second->output_id);
        CHECK(n.id == elm.second->id);
        CHECK(n.leak == elm.second->leak);
        CHECK(n.threshold == elm.second->threshold);

        CHECK(std::find(nl.begin(), nl.end(), elm.first) != nl.end());

        REQUIRE(n.synapses.size() == elm.second->synapses.size());
        for(auto syn : elm.second->synapses)
        {
            REQUIRE(snet.is_synapse(syn.first, elm.first));

            Synapse &s = snet.get_synapse(syn.first, elm.first);
            CHECK(s.weight == syn.second.weight);
            CHECK(s.delay == syn.second.delay);

            auto sp = std::make_pair(syn.first, elm.first);
            CHECK(std::find(sl.begin(), sl.end(), sp) != sl.end());
        }
    }
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
