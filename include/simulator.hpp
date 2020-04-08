#pragma once
#include <array>
#include <vector>
#include <utility>
#include <queue>
#include <fstream>
#include <stdexcept>

#include "network.hpp"
#include "backend.hpp"
#include "constants.hpp"

namespace caspian
{

    /* Internal fire events are quite lightweight. Each synapse is associated with a specific 
     * pre-synaptic neuron, and the time is implicitly stored as a relative value based on the 
     * which queue is used in the circular buffer. */
    struct FireEvent
    {
        Synapse *syn;     // where did the fire come from
        Neuron  *neuron;  // where does the fire go

        FireEvent() = delete;
        FireEvent(Synapse *s, Neuron *n) : syn(s), neuron(n) {}
        FireEvent(const FireEvent &e) = default;
        FireEvent(FireEvent &&e) = default;
        ~FireEvent() = default;

        FireEvent& operator=(const FireEvent &e) = default;
        FireEvent& operator=(FireEvent &&e) = default;
    };

    struct OutputMonitor
    {
        OutputMonitor(size_t n_outputs)
        {
            fire_counts.resize(n_outputs);
            last_fire_times.resize(n_outputs);
            recorded_fires.resize(n_outputs);
        }

        inline void add_fire(int id, uint64_t time, bool precise=false)
        {
            //if(id > int(fire_counts.size()))
            //    throw std::range_error("[Output Monitor] Provided output id exceed configuration");

            fire_counts[id] += 1;
            last_fire_times[id] = time;
            if(precise) recorded_fires[id].push_back(time);
        }
    
        inline void clear()
        {
            for(auto &c : fire_counts) c = 0;
            for(auto &t : last_fire_times) t = -1;
            for(auto &r : recorded_fires) r.clear();
        }

        std::vector<int> fire_counts;
        std::vector<uint64_t> last_fire_times;
        std::vector<std::vector<uint32_t>> recorded_fires;
    };

    /* The simluator implements the "Backend" interface. This simulator is single-threaded 
     * following a hybrid-event simulation model which loops through each timestep but only 
     * performs the necessary work at each step using a circular-buffer inspired event queue
     * data structure. This structure also serves as somewhat of an arena allocator for events
     * to also reduce malloc/heap allocation overhead and fragmentation. */
    class Simulator : public Backend
    {
    protected:

        /* processes a selected fire event */
        void process_fire(const FireEvent &e);
        void process_fire(const InputFireEvent &e);

        /* Updates last event & leak for a neuron */
        void refresh_neuron(Neuron *n);

        /* post-accumulation check for any neuron which may fire */
        void threshold_check(Neuron *elm);

        /* Neuron leak operation */
        void neuron_leak(Neuron *n);

        /* executes a single cycle of the simulation */
        void do_cycle();

        /* id -> element coordinates for inputs */
        std::vector<uint32_t> input_map;

        /* output monitoring config */
        std::vector<int64_t> monitor_aftertime;
        std::vector<bool> monitor_precise;

        /* output monitoring data */
        std::vector<OutputMonitor> output_logs;

        /* circular buffer of internal fire events */
        std::vector< std::vector<FireEvent> > fires;

        /* neurons which _might_ fire within the current cycle */
        std::vector<Neuron*> thresh_check;

        /* collection of input fires organized by time */
        std::vector<InputFireEvent> input_fires;

        /* hacked up spike raster */
        std::vector<std::vector<uint32_t>> all_spikes;

        /* stores the currently loaded network */
        std::vector<Network*> nets; // if multiple are loaded, all are here -- first is also stored in *net
        Network *net; // if only one is loaded, it is here

        /* metrics for Neuro GetMetric() */
        uint64_t metric_timesteps = 0;
        int metric_accumulates = 0;
        int metric_fires = 0;

        /* Network time at the start of a simulation call */
        uint64_t run_start_time = 0;

        /* Current network time */
        uint64_t net_time = 0;

        /* Information about the loaded network */
        uint16_t max_delay = 1;
        uint16_t dly_mask  = 0x1;
        bool soft_reset = false;
        bool multi_net_sim = false;

        /* collect all spikes? */
        bool collect_all = false;

        #ifdef TIMING
        std::map<std::string, int> meta;
        #endif

    public:
        Simulator();
        ~Simulator() = default;

        /* Queue fires into the array */
        void apply_input(int input_id, int16_t w, uint64_t t);

        /* Set the network to execute */
        bool configure(Network *network);
        bool configure_multi(std::vector<Network*>& networks);

        /* Simulate the network on the array for the specified timesteps */
        bool simulate(uint64_t steps);
        bool update();

        /* Get device metrics */
        double get_metric(const std::string &metric);
        uint64_t get_metric_uint(const std::string &metric);

        /* Get the current time */
        uint64_t get_time() const;

        /* pull the updated network -- has no meaning for Simulator -- network already has state */
        Network* pull_network(uint32_t idx) const;

        /* Methods of resetting sim and network state */
        void reset();
        void clear_activity();

        /* Track outputs */
        bool track_aftertime(uint32_t output_id, uint64_t aftertime);
        bool track_timing(uint32_t output_id, bool do_tracking = true);

        /* Get outputs from the simulation */
        int  get_output_count(uint32_t output_id, int network_id = 0);
        int  get_last_output_time(uint32_t output_id, int network_id = 0);
        std::vector<uint32_t> get_output_values(uint32_t output_id, int network_id = 0);

        void collect_all_spikes(bool collect = true); 
        std::vector<std::vector<uint32_t>> get_all_spikes(); 
    };
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
