#pragma once

#include <map>

#include "framework.hpp"
#include "backend.hpp"
#include "network.hpp"
#include "network_conversion.hpp"
#include "nlohmann/json.hpp"

namespace caspian
{
    using std::string;
    using std::vector;
    using neuro::Spike;
    using nlohmann::json;

    class Processor : public neuro::Processor 
    {
    public:
        Processor(json &j);
        ~Processor();

        // TODO: unique_ptr, shared_ptr, or raw pointer?
        bool load_network(neuro::Network* n, int network_id = 0);
        
        /* Apply spike(s) to a network */
        void apply_spike(const Spike& s, int network_id = 0);
        void apply_spikes(const vector<Spike>& s, int network_id = 0);

        /* Apply spike(s) to multiple networks */
        void apply_spike(const Spike& s, const vector<int>& network_ids);
        void apply_spikes(const vector<Spike>& s, const vector<int>& network_ids);

        /* Run the network for the desired time with queued input(s) */
        void run(double duration, int network_id = 0);
        void run(double duration, const vector<int>& network_ids);

        /* Get processor time based on specified network */
        double get_time(int network_id = 0);

        /* Output tracker */
        void track_aftertime(int output_id, double aftertime, int network_id = 0);
        void track_output(int output_id, bool track = true, int network_id = 0);

        /* Access output spike data */
        double output_last_fire(int output_id, int network_id = 0);
        int output_count(int output_id, int network_id = 0);
        vector<double> output_vector(int output_id, int network_id = 0);

        /* Remove the network from the processor */
        void clear(int network_id = 0);

        /* Remove state, keep network loaded */
        void clear_activity(int network_id = 0);

        /*** ADDED METHODS ***/
        caspian::Network* Get_Internal_Network(int network_id = 0) const;
        json Get_Configuration() const;
        caspian::Backend* Get_Backend() const;

    protected:
        caspian::Backend* dev;

        json jconfig;

        int loaded_network_id;
        neuro::Network* api_net;
        caspian::Network* internal_net;
    };

}

