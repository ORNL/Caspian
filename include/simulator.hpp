#pragma once
#include <array>
#include <vector>
#include <utility>
#include <queue>
#include <fstream>

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

        /* circular buffer of internal fire events */
        std::vector< std::vector<FireEvent> > fires;

        /* neurons which _might_ fire within the current cycle */
        std::vector<Neuron*> thresh_check;

        /* collection of input fires organized by time */
        std::vector<InputFireEvent> input_fires;

        /* Information about the loaded network */
        uint16_t max_delay = 1;
        uint16_t dly_mask  = 0x1;
        bool soft_reset = false;

        /* metrics for Neuro GetMetric() */
        int metric_accumulates = 0;
        int metric_fires = 0;
        uint64_t metric_timesteps = 0;

        /* Network time at the start of a simulation call */
        uint64_t run_start_time = 0;

        /* Current network time */
        uint64_t net_time = 0;

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

        /* Simulate the network on the array for the specified timesteps */
        bool simulate(uint64_t steps);
        bool update();

        /* Get device metrics */
        double get_metric(const std::string &metric);

        /* Get the current time */
        uint64_t get_time() const;

        /* pull the updated network -- has no meaning for Simulator -- network already has state */
        void pull_network(Network *net) const;

        /* Methods of resetting sim and network state */
        void reset();
        void clear_activity();
    };
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
