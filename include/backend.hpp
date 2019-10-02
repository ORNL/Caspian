#pragma once
#include <stdint.h>
#include <vector>
#include <map>

#include "constants.hpp"
#include "network.hpp"

namespace caspian
{

    /* Input fire events are different than an internal fire event because it does not 
     * involve a synapse and because it can be scheduled for any time. */
    struct InputFireEvent
    {
        uint32_t id;   // input id for "to" neuron
        uint64_t time;
        int16_t weight;

        InputFireEvent() = delete;
        InputFireEvent(uint32_t elm, int16_t w, uint64_t t) : id(elm), time(t), weight(w) {};
        InputFireEvent(const InputFireEvent &e) = default;
        InputFireEvent(InputFireEvent &&e) = default;
        ~InputFireEvent() = default;

        InputFireEvent& operator=(const InputFireEvent &e) = default;
        InputFireEvent& operator=(InputFireEvent &&e) = default;

        bool operator< (const InputFireEvent &rhs) const
        {
            return (time < rhs.time); // || (time == rhs.time && to < rhs.to);
        }

        bool operator> (const InputFireEvent &rhs) const
        {
            return (time > rhs.time); // || (time == rhs.time && !(to < rhs.to) && to != rhs.to);
        }

        bool operator== (const InputFireEvent &rhs) const
        {
            return (time == rhs.time); // && (to == rhs.to) && (weight == rhs.weight);
        }
    };

    /* Simulation interface for CASPIAN devices */
    class Backend
    {
    public:
        virtual bool configure(Network *network) = 0;
        virtual bool configure_multi(std::vector<Network*> &networks) = 0;
        virtual void pull_network(Network *network) const = 0;

        virtual void apply_input(int input_id, int16_t w, uint64_t t) = 0;
        virtual bool simulate(uint64_t steps) = 0;
        virtual bool update() = 0;

        virtual double get_metric(const std::string &metric) = 0;
        virtual uint64_t get_time() const = 0;

        virtual void reset() = 0;
        virtual void clear_activity() = 0;

        virtual bool track_aftertime(uint32_t output_id, uint64_t aftertime) = 0;
        virtual bool track_timing(uint32_t output_id, bool do_tracking = true) = 0;

        virtual int  get_output_count(uint32_t output_id, int network_id = 0) = 0;
        virtual int  get_last_output_time(uint32_t output_id, int network_id = 0) = 0;
        virtual std::vector<uint32_t> get_output_values(uint32_t output_id, int network_id = 0) = 0;

        virtual ~Backend() = default;
    };

}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
