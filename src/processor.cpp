#include "backend.hpp"
#include "simulator.hpp"
#include "constants.hpp"
#include "processor.hpp"
#include "fmt/format.h"
#include "utils/json_helpers.hpp"

using json = nlohmann::json;

static json specs = {
    { "Min_Threshold",      "I" },
    { "Max_Threshold",      "I" },
    { "Leak_Enable",        "B" },
    { "Min_Leak",           "I" },
    { "Max_Leak",           "I" },
    { "Min_Weight",         "I" },
    { "Max_Weight",         "I" },
    { "Min_Axon_Delay",     "I" },
    { "Max_Axon_Delay",     "I" },
    { "Min_Synapse_Delay",  "I" },
    { "Max_Synapse_Delay",  "I" }
};

namespace caspian
{
    using std::vector;
    using std::string;
    using fmt::format;
    using neuro::Spike;
    using neuro::Property;

    Processor::Processor(json& j)
    {
        dev = new Simulator();
        api_net = nullptr;
        internal_net = nullptr;

        // Default configuration
        jconfig = {
            { "Leak_Enable",            true },
            { "Min_Leak",               0 },
            { "Max_Leak",               constants::MAX_LEAK },
            { "Min_Threshold",          constants::MIN_THRESHOLD },
            { "Max_Threshold",          constants::MAX_THRESHOLD },
            { "Min_Weight",             constants::MIN_WEIGHT },
            { "Max_Weight",             constants::MAX_WEIGHT },
            { "Min_Axon_Delay",         constants::MIN_AXON_DELAY },
            { "Max_Axon_Delay",         constants::MAX_AXON_DELAY },
            { "Min_Synapse_Delay",      constants::MIN_DELAY },
            { "Max_Synapse_Delay",      constants::MAX_DELAY }
        };

        // Apply updates from provided configuration
        if(!j.empty())
            jconfig.update(j);

        // Check the configuration
        std::string json_chk = neuro::Parameter_Check_Json(jconfig, specs);

        // Cannot continue with an invalid configuration
        if(!json_chk.empty())
            throw std::runtime_error(json_chk);

        // Add neuron parameters
        properties.add_node_property("Threshold", jconfig["Min_Threshold"], jconfig["Max_Threshold"], neuro::Property::Type::INTEGER, 1);
        properties.add_node_property("Leak_Value", jconfig["Min_Leak"], jconfig["Max_Leak"], neuro::Property::Type::INTEGER, 1);
        properties.add_node_property("Delay", jconfig["Min_Axon_Delay"], jconfig["Max_Axon_Delay"], neuro::Property::Type::INTEGER, 1);

        // Add synapse parameters
        properties.add_edge_property("Weight",  jconfig["Min_Weight"], jconfig["Max_Weight"], neuro::Property::Type::INTEGER, 1);
        properties.add_edge_property("Delay", jconfig["Min_Synapse_Delay"], jconfig["Max_Synapse_Delay"], neuro::Property::Type::INTEGER, 1);
    }

    Processor::~Processor()
    {
        // delete our internal device pointer
        if(dev != nullptr)
            delete dev;

        if(internal_net != nullptr)
            delete internal_net;
    }

    neuro::PropertyPack Processor::get_properties()
    {
        return properties;
    }

    bool Processor::load_network(neuro::Network *n, int network_id)
    {
        // convert to internal_net
        if(internal_net != nullptr)
            delete internal_net;

        // keep the pointer
        api_net = n;
        
        // make a new shadow network
        internal_net = new Network();

        // convert to internal representation & handle the error case
        if(!network_framework_to_internal(n, internal_net))
        {
            // get rid of internal represntation for bad network
            delete internal_net;
            internal_net = nullptr;

            // indicate no network is loaded
            loaded_network_id = -1;

            // save error message
            //throw std::runtime_error("Error converting given network to internal representation");

            // return failure
            return false;
        }

        // keep track of the specified network id
        loaded_network_id = network_id;

        // configure the device
        return dev->configure(internal_net);
    }

    bool Processor::load_networks(vector<neuro::Network*> &n)
    {
        // Enable multi-network batch mode
    }

    void Processor::apply_spike(const Spike &s, int network_id)
    {
        if(loaded_network_id != network_id)
            throw std::runtime_error(format("[apply] Specified network {} is not loaded", network_id));

        int16_t int_val = s.value * caspian::constants::MAX_DEVICE_INPUT;
        dev->apply_input(s.id, int_val, s.time);
    }

    void Processor::apply_spike(const Spike &s, const vector<int>& network_ids)
    {
        (void) s;
        (void) network_ids;
        throw std::invalid_argument("Batch spike is not supported");
    }

    void Processor::apply_spikes(const std::vector<Spike>& spikes, int network_id)
    {
        // Error check? Not currently needed
        for(const Spike &s : spikes)
            apply_spike(s, network_id);
    }

    void Processor::apply_spikes(const std::vector<Spike>& spikes, const vector<int>& network_ids)
    {
        // Error check? Not currently needed
        for(const Spike &s : spikes)
            apply_spike(s, network_ids);
    }

    void Processor::run(double duration, int network_id)
    {
        if(loaded_network_id != network_id)
            throw std::runtime_error(format("[run] Specified network {} is not loaded", network_id));

        dev->simulate(duration);
    }

    void Processor::run(double duration, const vector<int>& network_ids)
    {
        (void) duration;
        (void) network_ids;
        throw std::invalid_argument("Batch run is not supported");
    }

    double Processor::get_time(int network_id)
    {
        if(loaded_network_id != network_id)
            throw std::runtime_error(format("[get_time] Specified network {} is not loaded", network_id));

        return dev->get_time();
    }

    void Processor::track_aftertime(int output_id, double aftertime, int network_id)
    {
        if(network_id != loaded_network_id)
            throw std::runtime_error(format("[output] Specified network {} is not loaded", network_id));

        // time conversion
        uint64_t atime = aftertime;

        dev->track_aftertime(output_id, atime);
    }

    void Processor::track_output(int output_id, bool track, int network_id)
    {
        if(network_id != loaded_network_id)
            throw std::runtime_error(format("[output] Specified network {} is not loaded", network_id));

        dev->track_timing(output_id, track);
    }

    double Processor::output_last_fire(int output_id, int network_id)
    {
        if(network_id != loaded_network_id)
            throw std::runtime_error(format("[output] Specified network {} is not loaded", network_id));

        return static_cast<double>(dev->get_last_output_time(output_id));
    }

    int Processor::output_count(int output_id, int network_id)
    {
        if(network_id != loaded_network_id)
            throw std::runtime_error(format("[output] Specified network {} is not loaded", network_id));

        return dev->get_output_count(output_id);
    }

    vector<double> Processor::output_vector(int output_id, int network_id)
    {
        if(network_id != loaded_network_id)
            throw std::runtime_error(format("[output] Specified network {} is not loaded", network_id));

        std::vector<uint32_t> i_times = dev->get_output_values(output_id);
        return std::vector<double>(i_times.begin(), i_times.end());
    }

    /* Removes the network */
    void Processor::clear(int network_id)
    {
        // TODO: network_id
        if(network_id != loaded_network_id)
            throw std::runtime_error(format("[clear] Specified network {} is not loaded", network_id));

        api_net = nullptr;

        if(internal_net != nullptr)
        {
            delete internal_net;
            internal_net = nullptr;
        }

        dev->configure(nullptr);
        loaded_network_id = -1;
    }

    /* Clears the state of a network */
    void Processor::clear_activity(int network_id)
    {
        // TODO: network_id
        if(network_id != loaded_network_id)
            throw std::runtime_error(format("[clear_activity] Specified network {} is not loaded", network_id));

        dev->clear_activity();
    }

    caspian::Network* Processor::get_internal_network(int network_id) const
    {
        if(network_id != loaded_network_id)
            return nullptr;

        return internal_net;
    }

    caspian::Backend* Processor::get_backend() const
    {
        return dev;
    }

    json Processor::get_configuration() const
    {
        return jconfig;
    }

}
