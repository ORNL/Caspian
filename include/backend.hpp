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
        uint32_t to;
        uint64_t time;
        int16_t weight;

        InputFireEvent() = delete;
        InputFireEvent(uint32_t elm, int16_t w, uint64_t t) : to(elm), time(t), weight(w) {};
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
    protected:

        /* stores the currently loaded network */
        Network *net;

        /* id -> element coordinates for inputs */
        std::vector<uint32_t> input_map;

        /* output monitoring */
        std::vector<int> fire_counts;
        std::vector<uint64_t> last_fire_times;
        std::vector<int64_t> monitor_aftertime;
        std::vector<bool> monitor_precise;
        std::vector<std::vector<uint32_t>> recorded_fires;

    public:
        virtual bool configure(Network *network) = 0;
        virtual void apply_input(int input_id, int16_t w, uint64_t t) = 0;

        virtual bool track_aftertime(uint32_t output_id, uint64_t aftertime);
        virtual bool track_timing(uint32_t output_id, bool do_tracking = true);

        virtual int  get_output_count(uint32_t output_id);
        virtual int  get_last_output_time(uint32_t output_id);
        virtual std::vector<uint32_t> get_output_values(uint32_t output_id);

        virtual bool simulate(uint64_t steps) = 0;
        virtual bool update() = 0;

        virtual double get_metric(const std::string &metric) = 0;

        virtual uint64_t get_time() const = 0;

        virtual void pull_network(Network *network) const = 0;

        virtual void reset() = 0;
        virtual void clear_activity() = 0;

        virtual ~Backend() = default;
    };

}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
