#include <iostream>
#include <sstream>
#include <random>
#include <cassert>

#include "nlohmann/json.hpp"

#include "fmt/format.h"
#include "fmt/ostream.h"
#include "network.hpp"
#include "constants.hpp"

namespace caspian
{

    std::string create_device_config(int size, int inputs, int outputs)
    {
        fmt::memory_buffer cfg;

        int maxdim = (inputs > outputs) ? inputs : outputs;
        size = (size > 1) ? size : maxdim * maxdim;
        size = (size > inputs + outputs) ? size : inputs + outputs;

        fmt::format_to(cfg, "size {}\n", size);

        for(int i = 0; i < inputs; i++)
            fmt::format_to(cfg, "I {0} {0}\n", i); 

        for(int i = 0; i < outputs; i++)
            fmt::format_to(cfg, "O {} {}\n", i, size-1-i);

        return fmt::to_string(cfg);
    }

    Neuron& Neuron::operator=(Neuron &&n) noexcept
    {
        if(&n == this) return *this;

        n.synapses = std::move(synapses);
        n.outputs = std::move(outputs);
        n.leak = leak;
        n.delay = delay;
        n.threshold = threshold;
        n.id = id;
        n.input_id = input_id;
        n.output_id = output_id;
        return *this;
    }

    nlohmann::json Neuron::to_json() const
    {
        nlohmann::json j;

        j["id"] = id;
        j["threshold"] = threshold;
        j["delay"] = delay;
        j["leak"] = leak;

        return j;
    }

    Network::Network(const Network &n)
    {
        m_max_size = n.m_max_size;
        m_inputs = n.m_inputs;
        m_outputs = n.m_outputs;
        m_neuron_ids = n.m_neuron_ids;
        m_synapse_pairs = n.m_synapse_pairs;
        m_num_synapses = n.m_num_synapses;
        m_time = 0;
        max_syn_delay = n.max_syn_delay;
        max_axon_delay = n.max_axon_delay;
        max_thresh = n.max_thresh;
        soft_reset = n.soft_reset;
        elements.clear();

        for(auto const &elm : n.elements)
            add_neuron(*(elm.second));

        // update element synapse pointers
        for(auto &&elm : elements)
        {
            for(std::pair<Neuron*, Synapse*> &p : elm.second->outputs)
            {
                p.first = & get_neuron(p.first->id);
                p.second = & get_synapse(elm.first, p.first->id);
            }
        }
    }

    Network& Network::operator=(const Network &n)
    {
        if(&n == this) return *this;

        m_max_size = n.m_max_size;
        m_inputs = n.m_inputs;
        m_outputs = n.m_outputs;
        m_neuron_ids = n.m_neuron_ids;
        m_synapse_pairs = n.m_synapse_pairs;
        m_num_synapses = n.m_num_synapses;
        m_time = 0;
        max_syn_delay = n.max_syn_delay;
        max_axon_delay = n.max_axon_delay;
        max_thresh = n.max_thresh;
        soft_reset = n.soft_reset;
        elements.clear();

        for(auto const &elm : n.elements)
            add_neuron(*(elm.second));

        // update element synapse pointers
        for(auto &&elm : elements)
        {
            for(std::pair<Neuron*, Synapse*> &p : elm.second->outputs)
            {
                p.first = & get_neuron(p.first->id);
                p.second = & get_synapse(elm.first, p.first->id);
            }
        }

        return *this;
    }

    Network::Network(Network &&n) noexcept
    {
        /* move inputs, outputs, and elements */
        m_inputs = std::move(n.m_inputs);
        m_outputs = std::move(n.m_outputs);
        m_neuron_ids = std::move(n.m_neuron_ids);
        m_synapse_pairs = std::move(n.m_synapse_pairs);
        elements = std::move(n.elements);

        /* copy stats */
        m_num_synapses = n.m_num_synapses;
        m_max_size = n.m_max_size;
        max_syn_delay = n.max_syn_delay;
        max_axon_delay = n.max_axon_delay;
        max_thresh = n.max_thresh;
        soft_reset = n.soft_reset;
        m_time = n.m_time;

        /* zero out stats in source network to invalidate it */
        n.m_max_size = 0;
        n.m_num_synapses = 0;
        n.m_time = 0;
    }

    Network& Network::operator=(Network &&n) noexcept
    {
        if(&n == this) return *this;

        /* move inputs, outputs, and elements */
        m_inputs = std::move(n.m_inputs);
        m_outputs = std::move(n.m_outputs);
        m_neuron_ids = std::move(n.m_neuron_ids);
        m_synapse_pairs = std::move(n.m_synapse_pairs);
        elements = std::move(n.elements);

        /* copy stats */
        m_num_synapses = n.m_num_synapses;
        m_max_size = n.m_max_size;
        max_syn_delay = n.max_syn_delay;
        max_axon_delay = n.max_axon_delay;
        max_thresh = n.max_thresh;
        soft_reset = n.soft_reset;
        m_time = n.m_time;

        /* zero out stats in source network to invalidate it */
        n.m_max_size = 0;
        n.m_num_synapses = 0;
        n.m_time = 0;

        return *this;
    }

    Network* Network::copy() const
    {
        return new Network(*this);
    }

    uint32_t Network::get_max_size() const
    {
        return m_max_size;
    }

    uint64_t Network::get_time() const
    {
        return m_time;
    }

    void Network::set_time(uint64_t t)
    {
        m_time = t;
    }

    void Network::reset()
    {
        m_time = 0;

        for(auto elm = elements.begin(); elm != elements.end(); ++elm)
        {
            // reset neuron charge
            elm.value()->charge = 0;
            elm.value()->tcheck = false;

            // reset last fire attribute
            elm.value()->last_fire = constants::MAX_TIME;
            elm.value()->last_event = constants::MAX_TIME;

            for(auto &&syn : elm.value()->synapses)
            {
                syn.second.last_fire = constants::MAX_TIME;
            }
        }
    }

    void Network::clear_activity()
    {
        m_time = 0;

        for(auto elm = elements.begin(); elm != elements.end(); ++elm)
        {
            // reset neuron charge
            elm.value()->charge = 0;
            elm.value()->tcheck = false;

            // reset last fire attribute
            elm.value()->last_fire = constants::MAX_TIME;
            elm.value()->last_event = constants::MAX_TIME;
        }
    }

    bool Network::is_neuron(uint32_t nid) const
    {
        return (elements.find(nid) != elements.end());
    }

    void Network::add_neuron(uint32_t nid, int16_t thresh, int8_t leak, uint8_t delay)
    {
        if(!is_neuron(nid))
        {
            elements.emplace(nid, new Neuron(thresh, nid, leak, delay));
            m_neuron_ids.push_back(nid);
        }
        else
        {
            Neuron &n = get_neuron(nid);
            n.threshold = thresh;
            n.leak = leak;
            n.delay = delay;
        }

        if(delay > max_axon_delay)
            max_axon_delay = delay;
    }

    void Network::add_neuron(Neuron &n)
    {
        Neuron *nn = new Neuron(n);

        // remove neuron if already exists
        if(is_neuron(n.id))
            remove_neuron(n.id);

        elements.emplace(nn->id, nn);
        m_neuron_ids.push_back(nn->id);
    }

    void Network::add_neuron(nlohmann::json &n)
    {
        uint32_t nid;
        int16_t thresh;
        int8_t leak = -1;
        uint8_t delay = 0;

        if(!n.contains("id") || !n.contains("threshold"))
        {
            throw std::invalid_argument("json values missing for neuron");
        }

        nid = n.at("id").get<int>();
        thresh = n.at("threshold").get<int>();

        if(n.contains("leak"))
            leak = n.at("leak").get<int>();

        if(n.contains("delay"))
            delay = n.at("delay").get<int>();

        add_neuron(nid, thresh, leak, delay);
    }

    bool Network::remove_neuron(uint32_t nid)
    {
        // early exit if neuron does not exist
        if(!is_neuron(nid)) return false;

        Neuron &n = get_neuron(nid);

        // remove all output synapses
        while(!n.outputs.empty())
            remove_synapse(nid, n.outputs.back().first->id);

        // remove all input synapses
        while(!n.synapses.empty())
            remove_synapse(n.synapses.begin()->first, nid);

        // remove neuron id from vector
        auto it = std::find(m_neuron_ids.begin(), m_neuron_ids.end(), nid);
        if(it != m_neuron_ids.end())
        {
            std::iter_swap(it, m_neuron_ids.end()-1);
            m_neuron_ids.pop_back();
        }

        // delete allocated memory
        delete elements.at(nid);

        // remove entry from hash table
        elements.erase(nid);

        return true;
    }

    Neuron& Network::get_neuron(uint32_t nid) const
    {
        auto it = elements.find(nid);

        if(it == elements.end())
        {
            throw std::runtime_error(fmt::format("Could not find neuron with id {} (total elements {})\n", nid, elements.size()));
        }

        return *(it.value());
    }

    Neuron* Network::get_neuron_ptr(uint32_t nid) const
    {
        auto it = elements.find(nid);

        if(it == elements.end())
            return nullptr;

        return it.value();
    }

    void Network::set_input(uint32_t nid, size_t id)
    {
        if(id >= m_inputs.size())
            m_inputs.resize(id+1, -1);

        if(is_neuron(nid))
        {
            get_neuron(nid).input_id = id;
            m_inputs[id] = nid;
        }
        else
        {
            fmt::print(std::cerr, "Neuron {} does not exist for input {}\n", nid, id);
        }
    }

    void Network::set_output(uint32_t nid, size_t id)
    {
        if(id >= m_outputs.size())
            m_outputs.resize(id+1, -1);

        if(is_neuron(nid))
        {
            get_neuron(nid).output_id = id;
            m_outputs[id] = nid;
        }
        else
        {
            fmt::print(std::cerr, "Neuron {} does not exist for output {}\n", nid, id);
        }
    }

    uint32_t Network::get_input(size_t id) const
    {
        return m_inputs.at(id);
    }

    uint32_t Network::get_output(size_t id) const
    {
        return m_outputs.at(id);
    }

    size_t Network::num_inputs() const
    {
        return m_inputs.size();
    }

    size_t Network::num_outputs() const
    {
        return m_outputs.size();
    }

    bool Network::is_synapse(uint32_t from, uint32_t to) const
    {
        if(elements.find(to) == elements.end())
            return false;

        return (elements.at(to)->synapses.find(from) != elements.at(to)->synapses.end());
    }

    void Network::add_synapse(uint32_t from, uint32_t to, int16_t w, uint8_t dly)
    {
        if(!is_synapse(from, to))
        {
            // add synapse to post-synaptic neuron
            Neuron &post_n = get_neuron(to);
            post_n.synapses.emplace(from, Synapse(w, dly));

            // add synapse to pre-synaptic neuron
            Neuron &pre_n = get_neuron(from);
            Synapse &s = get_synapse(from, to);
            pre_n.outputs.emplace_back(&post_n, &s);

            // add to list of synapses
            m_synapse_pairs.push_back(std::make_pair(from, to));

            // increment synapse count
            ++m_num_synapses;
        }
        else
        {
            // if the synapse exists, update values
            Synapse &s = get_synapse(from, to);
            s.weight = w;
            s.delay = dly;
        }

        if(dly > max_syn_delay)
            max_syn_delay = dly;
    }

    void Network::add_synapse(nlohmann::json &s)
    {
        uint32_t from, to;
        int16_t w;
        uint8_t dly = 0;

        if(!s.contains("from") || !s.contains("to") || !s.contains("weight"))
        {
            throw std::invalid_argument("json values missing for synapse");
        }

        from = s.at("from").get<int>();
        to = s.at("to").get<int>();
        w = s.at("weight").get<int>();

        if(s.contains("delay"))
        {
            dly = s.at("delay").get<int>();
        }

        add_synapse(from, to, w, dly);
    }

    bool Network::remove_synapse(uint32_t from, uint32_t to)
    {
        if(!is_synapse(from, to)) return false;

        Neuron* n = get_neuron_ptr(from);
        Neuron* t = get_neuron_ptr(to);
        Synapse* s = get_synapse_ptr(from, to);

        for(size_t i = 0; i < n->outputs.size(); ++i)
        {
            if(n->outputs[i].first == t && n->outputs[i].second == s)
            {
                // swap pairs to end
                std::swap(n->outputs[i], n->outputs[n->outputs.size()-1]);

                // pop pair
                n->outputs.pop_back();
                break;
            }
        }

        // remove synapse pair from vector
        auto it = std::find(m_synapse_pairs.begin(), m_synapse_pairs.end(), std::make_pair(from, to));
        if(it != m_synapse_pairs.end())
        {
            std::iter_swap(it, m_synapse_pairs.end()-1);
            m_synapse_pairs.pop_back();
        }

        t->synapses.erase(from);
        --m_num_synapses;

        return true;
    }

    Synapse& Network::get_synapse(uint32_t from, Neuron &to) const
    {
        return to.synapses.at(from);
    }

    Synapse& Network::get_synapse(uint32_t from, uint32_t to) const
    {
        return get_neuron(to).synapses.at(from);
    }

    Synapse* Network::get_synapse_ptr(uint32_t from, uint32_t to) const
    {
        Neuron *n = get_neuron_ptr(to);
        if(n == nullptr)
            return nullptr;

        return &(n->synapses.at(from));
    }

    double Network::get_metric(const std::string &metric)
    {
        double m = 0;

        if(metric == "neuron_count")
        {
            m = elements.size();
        }
        else if(metric == "synapse_count")
        {
            m = m_num_synapses;
        }
        else if(metric == "inhibitory_synapse_count")
        {
            m = negative_synapses();
        }
        else if(metric == "excitatory_synapse_count")
        {
            m = positive_synapses();
        }
        else
        {
            fmt::print(std::cerr, "Specified network metric '{}' is not implemented\n", metric);
        }

        return m;
    }

    void Network::from_str(const std::string &s)
    {
        // Create a Network from the serialization
        std::istringstream ss(s);
        from_stream(ss);
    }

    void Network::from_stream(std::istream &ss)
    {
        nlohmann::json j;
        ss >> j;
        from_json(j);
    }

    std::string Network::to_str() const
    {
        std::ostringstream ss;
        to_stream(ss);
        return ss.str();
    }

    nlohmann::json Network::to_json() const
    {
        nlohmann::json j;

        j["version"] = constants::FORMAT_VER;

        // i/o ids
        j["inputs"] = m_inputs;
        j["outputs"] = m_outputs;

        // configuration data
        j["config"]["soft_reset"] = soft_reset;
        j["config"]["max_syn_delay"] = max_syn_delay;
        j["config"]["max_axon_delay"] = max_axon_delay;
        j["config"]["max_threshold"] = max_thresh;

        // neurons
        j["neurons"] = nlohmann::json::array();
        for(auto const &elm : elements)
        {
            j["neurons"].push_back(elm.second->to_json());
        }

        // synapses
        j["synapses"] = nlohmann::json::array();
        for(const auto &s : m_synapse_pairs)
        {
            Synapse &syn = get_synapse(s.first, s.second);
            
            j["synapses"].push_back(nlohmann::json::object({
                {"from", s.first},
                {"to", s.second},
                {"weight", syn.weight},
                {"delay", syn.delay}
            }));
        }
        
        return j;
    }

    bool Network::from_json(nlohmann::json &j)
    {
        // initial check
        if(!j.contains("version") || !j.contains("neurons") || !j.contains("synapses"))
        {
            return false;
        }

        if(j["version"] < constants::FORMAT_VER)
        {
            return false;
        }

        // Clear network
        purge_elements();

        // Load neurons
        for(nlohmann::json &n : j["neurons"])
        {
            add_neuron(n);
        }

        // Load synapses
        for(nlohmann::json &s : j["synapses"])
        {
            add_synapse(s);
        }

        // inputs
        if(j.contains("inputs") && j["inputs"].is_array())
        {
            int idx = 0;
            for(auto &&inp : j["inputs"])
            {
                int value = inp.get<int>();
                set_input(value, idx);
                idx++;
            }
        }

        if(j.contains("outputs") && j["outputs"].is_array())
        {
            int idx = 0;
            for(auto &&outp : j["outputs"])
            {
                int value = outp.get<int>();
                set_output(value, idx);
                idx++;
            }
        }

        return true;
    }

    void Network::to_stream(std::ostream &ss) const
    {
        ss << to_json().dump(2) << std::endl;
    }

    std::string Network::to_gml() const
    {
        fmt::memory_buffer gml;

        fmt::format_to(gml, "graph [\n");
        fmt::format_to(gml, "  comment \"Automatically generated GML for CASPIAN\"\n");
        fmt::format_to(gml, "  label \"network\"\n");
        fmt::format_to(gml, "  directed 1\n");

        for(const auto &n : elements)
        {
            Neuron *np = n.second;

            fmt::format_to(gml, "  node [\n    id {0}\n    label {0}\n    threshold {1}\n  ]\n",
                    np->id, np->threshold);
        }

        for(const auto &n : elements)
        {
            for(auto s : n.second->synapses)
            {
                fmt::format_to(gml, "  edge [\n    source {0}\n    target {1}\n    weight {2}\n    delay {3}\n  ]\n",
                        s.first, n.first, s.second.weight, s.second.delay);
            }
        }

        fmt::format_to(gml, "]\n");

        return fmt::to_string(gml);
    }

    void Network::prune(bool io_prune)
    {
        // create a list of neurons to remove
        std::set<uint32_t> remove_list;

        // recursive lambda implementation of DFS
        std::function<void(Neuron*)> traverse_outputs;
        traverse_outputs = [&traverse_outputs](Neuron *n) {
            // use charge variable to indicate visited or not     
            if(n->charge > 0) return;

            // label as visited
            n->charge = 1;

            // traverse connections
            for(auto output : n->outputs)
                traverse_outputs(output.first);
        };

        // recursive lambda implementation of DFS
        std::function<void(Neuron*)> traverse_inputs;
        traverse_inputs = [&traverse_inputs, this](Neuron *n) {
            // use charge variable to indicate visited or not     
            if(n->charge > 0) return;

            // label as visited
            n->charge = 1;

            // traverse connections
            for(auto input : n->synapses)
                traverse_inputs(elements.at(input.first));
        };

        // initially reset state 
        reset();

        // DFS from each input
        for(auto c : m_inputs)
            if(is_neuron(c))
                traverse_outputs(elements.at(c));

        // check each neuron to see if it was visited in the search
        for(auto elm : elements)
            if(elm.second->charge == 0 && (io_prune || (elm.second->input_id == -1 && elm.second->output_id == -1)))
                remove_list.insert(elm.first);

        // remove all the extra neurons
        for(auto c : remove_list)
            remove_neuron(c);

        // return the network to a reset state to remove added charge
        reset();

        // clear the removal set
        remove_list.clear();

        // DFS backwards for each output
        for(auto c : m_outputs)
            if(is_neuron(c))
                traverse_inputs(elements.at(c));

        // check each neuron to see if it was visited in the search
        for(auto elm : elements)
            if(elm.second->charge == 0 && (io_prune || (elm.second->input_id == -1 && elm.second->output_id == -1)))
                remove_list.insert(elm.first);

        // remove all the extra neurons
        for(auto c : remove_list)
            remove_neuron(c);

        // return the network to a reset state to remove added charge
        reset();
    }

    NeuronTable::iterator Network::begin()
    {
        return elements.begin();
    }

    NeuronTable::iterator Network::end()
    {
        return elements.end();
    }

    size_t Network::size() const
    {
        return elements.size();
    }

    size_t Network::num_neurons() const
    {
        return elements.size();
    }

    size_t Network::num_synapses() const
    {
        return m_num_synapses;
    }

    Network::~Network()
    {
        for(auto elm : elements)
            delete elm.second;
    }

    void Network::purge_elements()
    {
        for(auto elm : elements)
            delete elm.second;

        elements.clear();

        m_num_synapses = 0;
    }

    int Network::positive_synapses() const
    {
        int cnt = 0;

        for(auto elm : elements)
            for(auto syn : elm.second->synapses)
                if(syn.second.weight > 0)
                    cnt++;

        return cnt;
    }

    int Network::negative_synapses() const
    {
        int cnt = 0;

        for(auto elm : elements)
            for(auto syn : elm.second->synapses)
                if(syn.second.weight < 0)
                    cnt++;

        return cnt;
    }

    uint32_t Network::get_random_input() const
    {
        if(m_neuron_ids.size() == 0) return 0;
        int r = rand() % m_inputs.size();        
        return m_inputs.at(r);
    }

    uint32_t Network::get_random_output() const
    {
        if(m_neuron_ids.size() == 0) return 0;
        int r = rand() % m_outputs.size();
        return m_outputs.at(r);
    }

    uint32_t Network::get_random_neuron(bool /*only_hidden*/) const
    {
        if(m_neuron_ids.size() <= 1) return 0;
        int r = rand() % m_neuron_ids.size();
        return m_neuron_ids.at(r);
    }

    std::pair<uint32_t, uint32_t> Network::get_random_synapse() const
    {
        if(m_neuron_ids.size() <= 1) return std::make_pair(0,0);
        int r = rand() % m_synapse_pairs.size();
        return m_synapse_pairs.at(r);
    }

    std::vector<uint32_t> Network::get_neuron_list() const
    {
        return m_neuron_ids;
    }

    std::vector<std::pair<uint32_t, uint32_t>> Network::get_synapse_list() const
    {
        return m_synapse_pairs;
    }

    bool Network::operator==(const Network &rhs) const
    {
        // check sizes
        if(num_neurons() != rhs.num_neurons()) return false;
        if(num_synapses() != rhs.num_synapses()) return false;
        if(num_inputs() != rhs.num_inputs()) return false;
        if(num_outputs() != rhs.num_outputs()) return false;
       
        // check config
        if(max_syn_delay != rhs.max_syn_delay) return false;
        if(max_axon_delay != rhs.max_axon_delay) return false;
        if(max_thresh != rhs.max_thresh) return false;
        if(soft_reset != rhs.soft_reset) return false;

        // check all neurons
        for(int nid : m_neuron_ids)
        {
            if(!rhs.is_neuron(nid)) return false;

            Neuron &na = get_neuron(nid);
            Neuron &nb = rhs.get_neuron(nid);

            if(na.threshold != nb.threshold) return false;
            if(na.leak != nb.leak) return false;
            if(na.input_id != nb.input_id) return false;
            if(na.output_id != nb.output_id) return false;
        }

        // check all synapses
        for(auto idpair : m_synapse_pairs)
        {
            uint32_t from = idpair.first;
            uint32_t to = idpair.second;

            if(!rhs.is_synapse(from, to)) return false;

            Synapse &sa = get_synapse(from, to);
            Synapse &sb = rhs.get_synapse(from, to);

            if(sa.weight != sb.weight) return false;
            if(sa.delay != sb.delay) return false;
        }

        return true;
    }

    void Network::make_random(int n_inputs, 
            int n_outputs,
            uint64_t seed,
            int n_input_synapses, 
            int n_output_synapses, 
            int n_hidden_synapses, 
            int n_hidden_synapses_max,
            double inhibitory_percentage,
            std::pair<int,int> threshold_range,
            std::pair<int,int> leak_range,
            std::pair<int,int> weight_range,
            std::pair<int,int> delay_range)
    {
        int i, j, fr, to;
        int n_neurons = m_max_size;
        int n_hidden_neurons = n_neurons - n_inputs - n_outputs;

        int start_outputs = n_inputs;
        int end_outputs = n_inputs + n_outputs;

        std::mt19937_64 rand_engine(seed);

        // flush out anything that might already be in this network
        purge_elements();

        if(n_input_synapses == -1)
            n_input_synapses = 12;

        if(n_output_synapses == -1)
            n_output_synapses = 12;

        if(n_hidden_synapses == -1)
            n_hidden_synapses = 6;

        if(n_hidden_synapses_max == -1)
            n_hidden_synapses_max = n_hidden_synapses * 1.2;

        auto randint = [&rand_engine](int imin, int imax) -> int
        {
            return (rand_engine() % (1+imax-imin)) + imin;
        };

        auto randfloat = [&rand_engine](double fmin, double fmax) -> double
        {
            double r = rand_engine();
            double rm = rand_engine.max();
            double scale = fmax - fmin;
            return ((r/rm) * scale) + fmin;
        };

        auto rand_syn = [&](int fr, int to) 
        {
            // 1/4 chance of -1 (inhibitory), 3/4 chance of 1 (excitatory)
            //int sign = (randint(0,3) != 0) ? 1 : -1;
            int sign = (randfloat(0.0f, 1.0f) < inhibitory_percentage) ? -1 : 1;
            int weight = randint(weight_range.first, weight_range.second) * sign;
            int delay = randint(delay_range.first, delay_range.second);
            add_synapse(fr, to, weight, delay);
        };

        // create neurons
        for(i = 0; i < n_neurons; ++i)
        {
            int threshold = randint(threshold_range.first, threshold_range.second); // randomize
            int leak = randint(leak_range.first, leak_range.second); // randomize
            add_neuron(i, threshold, leak);
        }

        // set up inputs
        for(i = 0; i < n_inputs; ++i)
            set_input(i, i);

        // set up outputs
        for(i = 0; i < n_outputs; ++i)
            set_output(start_outputs + i, i);

        // inputs -> hidden synapses
        for(i = 0; i < n_inputs; ++i)
        {
            for(j = 0; j < n_input_synapses; ++j)
            {
                to = randint(end_outputs, n_neurons-1); // find random neuron
                rand_syn(i, to);
            }
        }

        // hidden -> output synapses
        for(i = 0; i < n_outputs; ++i)
        {
            for(j = 0; j < n_output_synapses; ++j)
            {
                fr = randint(end_outputs, n_neurons-1); // find random neuron
                rand_syn(fr, start_outputs+i);
            }
        }

        // hidden -> hidden synapses
        for(i = 0; i < n_hidden_neurons; ++i)
        {
            fr = n_inputs + i;

            for(j = 0; j < n_hidden_synapses; ++j)
            {
                do {
                    to = randint(end_outputs, n_neurons-1); // find random neuron
                } while(fr == to);

                if(elements.at(to)->synapses.size() < static_cast<size_t>(n_hidden_synapses_max))
                    rand_syn(fr, to);
            }
        }
    }

}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
