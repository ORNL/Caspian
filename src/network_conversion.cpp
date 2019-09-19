#include "network_conversion.hpp"
#include "fmt/format.h"

namespace caspian
{
    static const bool s_debug = false;

    bool network_framework_to_internal(neuro::Network *tn, capsian::Network *net)
    {
        using neuro::Node;
        using neuro::Edge;

        // index for each parameter in the appropriate data vector
        int threshold_id, leak_id, leak_en_id, neuron_delay_id;
        int weight_id, delay_id;

        // Sanity check pointers
        if(tn == nullptr || net == nullptr)
            return false;

        if(s_debug) fmt::print("Start new conversion\n");

        // Get definition indicies for Data Vectors
        threshold_id = get_def_index(tn->Node_Spec, "Threshold");
        leak_id = get_def_index(tn->Node_Spec, "Leak_Value");
        leak_en_id = get_def_index(tn->Node_Spec, "Leak_Enable");
        neuron_delay_id = get_def_index(tn->Node_Spec, "Delay");
        weight_id = get_def_index(tn->Edge_Spec, "Weight");
        delay_id = get_def_index(tn->Edge_Spec, "Delay");

        // check if essential properties exist
        if(threshold_id < 0 || weight_id < 0)
            return false;
    
        // Add neurons
        for(Node* node : tn->All_Nodes)
        {
            int nid = node->Index;
            int threshold = node->Data->Vals[threshold_id].ll;
            int delay = 0;
            int leak = -1;
            bool leak_enable = false;

            if(neuron_delay_id >= 0) delay = node->Data->Vals[neuron_delay_id].ll;
            if(leak_id >= 0) leak = node->Data->Vals[leak_id].ll;
            if(leak_en_id >= 0) leak_enable = (node->Data->Vals[leak_en_id].ll != 0);
            if(!leak_enable) leak = -1;

            net->add_neuron(nid, threshold, leak, delay);

            if(node->Input >= 0)
                net->set_input(nid, node->Input);

            if(node->Output >= 0)
                net->set_output(nid, node->Output);

            if(s_debug) fmt::print("Add neuron {} -- threshold: {}, delay: {}, in: {}, out: {}\n", nid, threshold, delay, node->Input, node->Output);
        }

        // Add synapses
        for(Edge* edge : tn->All_Edges)
        {
            int from = edge->From->Index;
            int to = edge->To->Index;
            int weight = edge->Data->Vals[weight_id].ll;
            int delay = 0;
            
            if(delay_id >= 0) delay = edge->Data->Vals[delay_id].ll; 

            if(s_debug) fmt::print("Add Synapse {} -> {} -- weight: {}, delay: {}\n", from, to, weight, delay);

            try {
                net->add_synapse(from, to, weight, delay);
            } catch(std::runtime_error &e) {
                fmt::print("Caught & ignoring exception: {}", e.what());
                fmt::print(" SYNAPSE {} -> {}\n", from, to);
            }
        }

        return true;
    }


    bool internal_network_to_tennlab(caspian::Network* /*net*/, TENNLab::Network* /*tn*/)
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
