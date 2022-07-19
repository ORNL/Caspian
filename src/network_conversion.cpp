#include "framework.hpp"
#include "network.hpp"
#include "network_conversion.hpp"
#include "fmt/format.h"

namespace caspian
{
    static const bool s_debug = false;

    bool network_framework_to_internal(neuro::Network *tn, caspian::Network *net)
    {
        using neuro::Property;

        (void) s_debug;

        if(tn == nullptr || net == nullptr)
            return false;

        const Property* node_threshold = tn->get_node_property("Threshold");
        const Property* node_leak = nullptr;
        const Property* node_delay = nullptr;

        if(tn->is_node_property("Leak"))
            node_leak = tn->get_node_property("Leak");

        if(tn->is_node_property("Delay"))
            node_delay = tn->get_node_property("Delay");

        const Property* edge_weight = tn->get_edge_property("Weight");
        const Property* edge_delay = tn->get_edge_property("Delay");

        for(auto nit = tn->begin(); nit != tn->end(); ++nit)
        {
            neuro::Node* node = nit->second.get();

            int nid = nit->first;
            int threshold = node->values[node_threshold->index];
            int leak = -1;
            int delay = 0;

            if(node_leak != nullptr) leak = node->values[node_leak->index];
            if(node_delay != nullptr) delay = node->values[node_delay->index];

            net->add_neuron(nid, threshold, leak, delay);

            if(node->input_id >= 0)
                net->set_input(nid, node->input_id);

            if(node->output_id >= 0)
                net->set_output(nid, node->output_id);
        }

        for(auto eit = tn->edges_begin(); eit != tn->edges_end(); ++eit)
        {
            int from = eit->first.first;
            int to = eit->first.second;
            int weight = eit->second->values[edge_weight->index];
            int delay = eit->second->values[edge_delay->index];

            net->add_synapse(from, to, weight, delay);
        }

        return true;
    }


    bool internal_network_to_tennlab(caspian::Network* /*net*/, neuro::Network* /*tn*/)
    {
        // TODO

        // Get Node_Spec / Edge_Spec / Network_Spec set up 
        
        // Transfer Neurons

        // Transfer Synapses

        // Set Inputs

        // Set Outputs

        return false;
    }
}
