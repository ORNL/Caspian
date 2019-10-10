#pragma once
#include <cstdint>
#include <vector>
#include <set>
#include <map>
#include <stdexcept>

#include "nlohmann/json.hpp"
#include "robinhood/robin_map.h"
#include "constants.hpp"

#ifdef TIMING
#include <chrono>
#endif

namespace caspian
{
    /* Forward declare */
    struct Synapse;
    struct Neuron;
    class Network;

    /* Robin Hood Hash Table to hold the neurons for a network */
    typedef tsl::robin_map<uint32_t, Neuron*> NeuronTable;

    /* Creates a valid device configuration string */
    std::string create_device_config(int size, int inputs, int outputs);

    struct Synapse
    {
        Synapse() = default;
        Synapse(int16_t weight_, uint8_t delay_ = 0) noexcept : weight(weight_), delay(delay_) {}
        Synapse(const Synapse &s) = default;
        Synapse(Synapse &&s) = default;
        Synapse& operator=(const Synapse &s) = default;
        Synapse& operator=(Synapse &&s) = default;
        ~Synapse() = default;

        /* weight value of the synapse */
        int16_t  weight = 0;
        /* # of delay cycles in which the synapse delays a fire */
        uint8_t  delay = 0;
        /* time of the last fire */
        //uint64_t last_fire = 0;
    };

    struct Neuron
    {
        Neuron() = default;

        /* These consturctors are explicitly declared noexcept. This is an optimization but also 
         * potentially misleading. For example, std::bad_alloc could be thrown by the STL when
         * copying this structure. However, in such a circumstance, we are fine with the program crashing.
         */

        Neuron(int16_t threshold_, uint32_t id_ = 0, int8_t leak_ = -1, uint8_t delay_ = 0) noexcept : 
            id(id_),
            threshold(threshold_),
            leak(leak_),
            delay(delay_) {}
        
        Neuron(Neuron &&n) noexcept :
            synapses(std::move(n.synapses)),
            outputs(std::move(n.outputs)),
            id(n.id),
            input_id(n.input_id),
            output_id(n.output_id),
            tag(n.tag),
            threshold(n.threshold),
            leak(n.leak),
            delay(n.delay) {}

        Neuron& operator=(Neuron &&n) noexcept;

        ~Neuron() = default;

        nlohmann::json to_json() const;

        /* input synapses */
        std::map<uint32_t, Synapse> synapses;

        /* outputs */
        std::vector< std::pair<Neuron*, Synapse*> > outputs;

        /* time of last fire */
        //uint64_t   last_fire = constants::MAX_TIME;
        /* time of last fire event *into* this neuron */
        uint64_t   last_event = constants::MAX_TIME;
        /* Coordinates */
        uint32_t   id;
        /* IO id */
        int        input_id = -1;
        int        output_id = -1;
        /* meta */
        int        tag = -1;
        /* current stored charge from accumulated fires */
        int32_t    charge = 0;
        /* threshold before the neuron will fire */
        int16_t    threshold = 0;
        /* Queued for threshold check in simulator */
        bool       tcheck = false;
        /* leak configuration (neuron-level granularity) -- stored as exponent 2^x */
        int8_t     leak = -1;
        /* # of delay cycles for the neuron/axon */
        uint8_t    delay = 0;
        
        protected:
        /* Copying is problematic because Synapse* will be invalidated, so
         * this should only be used wittingly in the Network class. */
        Neuron(Neuron &n) noexcept : 
            synapses(n.synapses),
            outputs(n.outputs),
            id(n.id),
            input_id(n.input_id),
            output_id(n.output_id),
            threshold(n.threshold),
            leak(n.leak),
            delay(n.delay) {}

        /* Further, the assignment operator should not be used */
        Neuron& operator= (const Neuron& n) = delete;
        
        friend class Network;
    };

    class Network
    {
    public:
        /* Very standard constructor */
        Network(size_t max_size = 0) : m_max_size(max_size) {}

        /* Copy Constructor */
        Network(const Network &n);
        Network& operator=(const Network &n);

        /* Move Constructor */
        Network(Network &&n) noexcept;
        Network& operator=(Network &&n) noexcept;

        /* Destructor -- important -- it must free all allocated neurons */
        ~Network();

        /* Equality */
        bool operator==(const Network &rhs) const;

        /* Serialization methods */
        void                    from_str(const std::string &s);
        std::string             to_str() const;

        /* JSON methods */
        bool                    from_json(nlohmann::json &j);
        nlohmann::json          to_json() const;

        /* Convert to GML */
        std::string             to_gml() const;

        /* Stream Serialization methods -- more efficient than the string-based methods */
        void                    from_stream(std::istream &st);
        void                    to_stream(std::ostream &st) const;

        /* Misc functions */
        void                    reset();
        void                    clear_activity();
        size_t                  num_neurons() const;
        size_t                  num_synapses() const;
        void                    purge_elements();
        Network*                copy() const;
        void                    prune(bool io_prune = false);
        void                    make_random(int n_inputs, 
                                            int n_outputs, 
                                            uint64_t seed,
                                            int n_input_synapses = -1, 
                                            int n_output_synapses = -1, 
                                            int n_hidden_synapses = -1, 
                                            int n_hidden_synapses_max = -1,
                                            double inhibitory_percentage = 0.2,
                                            std::pair<int,int> threshold_range = std::make_pair(0,255),
                                            std::pair<int,int> leak_range = std::make_pair(0,3),
                                            std::pair<int,int> weight_range = std::make_pair(0,255),
                                            std::pair<int,int> delay_range = std::make_pair(0,15));

        /* Configuration functions */
        uint64_t                get_time() const;
        uint32_t                get_max_size() const;
        void                    set_time(uint64_t t);

        /* Neuron functions */
        bool                    is_neuron(uint32_t nid) const;
        void                    add_neuron(uint32_t nid, int16_t thresh, int8_t leak=-1, uint8_t delay = 0);
        void                    add_neuron(nlohmann::json &n);
        bool                    remove_neuron(uint32_t nid);
        Neuron&                 get_neuron(uint32_t nid) const;
        Neuron*                 get_neuron_ptr(uint32_t nid) const;

        /* Input/Output Neuron functions */
        void                    set_input(uint32_t nid, size_t id);
        void                    set_output(uint32_t nid, size_t id);
        uint32_t                get_input(size_t id) const;
        uint32_t                get_output(size_t id) const;
        size_t                  num_inputs() const;
        size_t                  num_outputs() const;

        /* Synapse functions*/
        bool                    is_synapse(uint32_t from, uint32_t to) const;
        void                    add_synapse(uint32_t from, uint32_t to, int16_t w, uint8_t dly = 0);
        void                    add_synapse(nlohmann::json &s);
        bool                    remove_synapse(uint32_t from, uint32_t to);
        Synapse&                get_synapse(uint32_t from, uint32_t to) const;
        Synapse&                get_synapse(uint32_t from, Neuron &to) const;
        Synapse*                get_synapse_ptr(uint32_t from, uint32_t to) const;

        /* Network metrics (i.e. neuron count) */
        double                  get_metric(const std::string &metric);

        /* "STL"-like data structure functionality */
        NeuronTable::iterator   begin();
        NeuronTable::iterator   end();
        size_t                  size() const;
        
        /* Psuedo-random Methods */
        uint32_t                get_random_input() const;
        uint32_t                get_random_output() const;
        uint32_t                get_random_neuron(bool only_hidden = false) const;
        std::pair<uint32_t, uint32_t> 
                                get_random_synapse() const;

        /* Return lists of ids */
        std::vector<uint32_t>   get_neuron_list() const;
        std::vector<std::pair<uint32_t, uint32_t>>
                                get_synapse_list() const;

        /* information about the configuration used with this network */
        uint16_t                max_thresh = constants::MAX_THRESHOLD;
        bool                    soft_reset = false;
        uint8_t                 max_syn_delay = 0; // constants::DEFAULT_MAX_DELAY;
        uint8_t                 max_axon_delay = 0; // constants::DEFAULT_MAX_DELAY;

    protected:
        /* hash table of all the neurons in the network */
        NeuronTable elements;
        /* association of input id to the neuron location */
        std::vector<int32_t> m_inputs;
        /* association of output id to the neuron location */
        std::vector<int32_t> m_outputs;

        /* track what we've got */
        std::vector<uint32_t> m_neuron_ids;
        std::vector<std::pair<uint32_t, uint32_t>> m_synapse_pairs;

        /* synapse metrics -- expensive run time cost */
        int positive_synapses() const;
        int negative_synapses() const;

        /* copying neurons can be problematic -- don't let the public do it */
        void add_neuron(Neuron &n);

        /* dimensions of the 'grid' of elements */
        size_t   m_max_size = 0;

        /* current network time */
        uint64_t m_time = 0;

        /* total number of synapses within the network */
        int      m_num_synapses = 0;
        
        friend class Simulator;
    };

}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
