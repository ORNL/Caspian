#include <iostream>
#include <chrono>
#include <algorithm>

#include "fmt/format.h"
#include "fmt/ostream.h"
#include "simulator.hpp"
#include "network.hpp"
#include "constants.hpp"

namespace caspian
{
    using constants::delay_bucket;

    template <typename T>
    static inline T clamp(T value, T min_value, T max_value)
    {
        return std::max(min_value, std::min(max_value, value));
    }

    void Simulator::refresh_neuron(Neuron *n)
    {
        int32_t imm = n->charge;

        // check and apply leak
        //   This approximates 2^(-t/tau) exponential leak
        //   only uses integer multiplication, addition/subtraction, and bit shifts
        //   along with a look up table with tau number of entries
        if(n->leak > -1 && net_time > n->last_event)
        {
            int t = net_time - n->last_event;
            int shamt = t >> n->leak;
            int t_masked = t & ((1 << n->leak)-1);
            
            imm = abs(imm);

            if(t_masked != 0)
            {
                int comp_idx = ((1 << n->leak) - t_masked) 
                               * (1 << (constants::MAX_LEAK - n->leak));

                imm = (imm * constants::LEAK_COMP[comp_idx]) >> constants::COMP_BITS;
            }

            imm >>= shamt;
            imm = (n->charge > 0) ? imm : -imm;
        }

        // update last_event time
        n->last_event = net_time;

        // clamp charge between [min, max]
        n->charge = clamp(imm, constants::MIN_CHARGE, constants::MAX_CHARGE);
    }

    void Simulator::process_fire(const InputFireEvent &e)
    {
        Neuron &to = net->get_neuron(e.to);

        // refresh the state of the neuron
        if(to.last_event != net_time)
            refresh_neuron(&to);

        // accumulate charge
        to.charge += e.weight;

        // increment accumulations count
        metric_accumulates++;

        // check threshold
        if(to.charge > to.threshold && !to.tcheck)
        {
            // add to list of elements to check
            thresh_check.emplace_back(&to);
            to.tcheck = true;
        }
    }

    void Simulator::process_fire(const FireEvent &e)
    {
        if(e.neuron->last_event != net_time)
            refresh_neuron(e.neuron);

        // accumulate charge
        e.neuron->charge += e.syn->weight;

        // increment accumulations count
        metric_accumulates++;

        // set synapse last fired
        e.syn->last_fire = net_time;

        // check threshold
        if(e.neuron->charge > e.neuron->threshold && !e.neuron->tcheck)
        {
            // add to list of elements to check
            thresh_check.emplace_back(e.neuron);
            e.neuron->tcheck = true;
        }
    }

    void Simulator::threshold_check(Neuron *n)
    {
        // reset tcheck status
        n->tcheck = false;

        if(n->charge > n->threshold)
        {
            // increment count of fires
            metric_fires++;

            // reset charge after firing (soft reset => charge - threshold, hard reset => 0)
            if(soft_reset) 
                n->charge -= n->threshold;
            else
                n->charge = 0;

            // todo: for soft_reset, if charge is still > threshold, schedule null event for t+1?

            // update last fired
            n->last_fire = net_time;

            // create a fire event for each output of the neuron
            for(std::pair<Neuron*, Synapse*> & opair : n->outputs)
            {
                Synapse *syn = opair.second;

                // schedule the event based on the current time plus any synaptic or axonal delay
                uint64_t fire_idx = delay_bucket(net_time + syn->delay + n->delay + 1, dly_mask);

                // add the fire event
                fires[fire_idx].emplace_back(syn, opair.first);
            }

            // monitor outputs
            //   --- output fires currently do *not* have axonal delay
            if(n->output_id >= 0)
            {
                // time since start of simulation call
                int64_t time_diff = net_time - run_start_time; 
                // is the current time after the specified starting time?
                bool after_start = (time_diff >= monitor_aftertime[n->output_id]);

                // check monitor times
                if(after_start)
                {
                    fire_counts[n->output_id] += 1;
                    last_fire_times[n->output_id] = net_time - run_start_time;

                    if(monitor_precise[n->output_id])
                        recorded_fires[n->output_id].push_back(net_time - run_start_time);
                }
            }
        }
    }

    void Simulator::do_cycle()
    {
        // process input fires
        while(!input_fires.empty() && input_fires.back().time == net_time)
        {
            process_fire(input_fires.back());
            input_fires.pop_back();
        }

        // determine bucket index => net_time % n_buckets
        size_t f_idx = delay_bucket(net_time, dly_mask);

        // process fire events in fire queue
        for(size_t i = 0; i < fires[f_idx].size(); ++i)
        {
            process_fire(fires[f_idx][i]);
        }

        // clear processed events all at once
        fires[f_idx].clear();

        // check thresholds after all fires are processed for the timestep
        for(size_t i = 0; i < thresh_check.size(); ++i)
        {
            threshold_check(thresh_check[i]);
        }

        // clear processed neurons all at once
        thresh_check.clear();
    }

    bool Simulator::configure(Network *n)
    {
        // clear all state variables inside simulation
        net_time = 0;
        input_map.clear();
        input_fires.clear();
        thresh_check.clear();

        // clear fire tracking
        fire_counts.clear();
        last_fire_times.clear();
        monitor_aftertime.clear();
        monitor_precise.clear();
        recorded_fires.clear();

        // clear internal fires
        for(auto &&f : fires)
            f.clear();

        // assign the network pointer
        net = n;

        // extract meaningful configuration if network is not null
        if(n != nullptr)
        {
            uint32_t thresh_bits = 0;
            uint32_t max_thresh = net->max_thresh + 1;
            while(max_thresh >>= 1)
                thresh_bits++;

            // neuron soft reset
            soft_reset = net->soft_reset;

            // set up input mapping
            input_map.resize(net->num_inputs());
            for(size_t i = 0; i < net->num_inputs(); i++)
                input_map[i] = net->get_input(i);

            // set up output monitoring
            fire_counts.resize(net->num_outputs(), 0);
            last_fire_times.resize(net->num_outputs(), -1);
            monitor_aftertime.resize(net->num_outputs(), -1);
            monitor_precise.resize(net->num_outputs(), false);
            recorded_fires.resize(net->num_outputs());

            // Get maxmimum delay from network & allocate enough schedule slots in circular buffer
            int total_max_delay = net->max_axon_delay + net->max_syn_delay;
            max_delay = constants::next_pow_of_2(total_max_delay+1)-1;
            dly_mask = max_delay;
            fires.resize(max_delay+1);
        }

        return true;
    }

    void Simulator::apply_input(int input_id, int16_t w, uint64_t t)
    {
        input_fires.emplace_back(net->get_input(input_id), w, net_time + t);
    }

    bool Simulator::simulate(uint64_t steps)
    {
        uint64_t end_time;

        // can't simulate if no network is configured
        if(net == nullptr)
            return false;

        // sort the inputs prior to starting simulation
        std::sort(input_fires.begin(), input_fires.end(), std::greater<InputFireEvent>());

        // clear fire tracking information
        for(auto &&c : fire_counts) c = 0;
        for(auto &&t : last_fire_times) t = -1;
        for(auto &&r : recorded_fires) r.clear();

        run_start_time = net->get_time();
        end_time = run_start_time + steps;

        // ok, not a strictly event-based system for now
        for(net_time = run_start_time; net_time < end_time; ++net_time)
            do_cycle();

        // save updated time to the network
        net->set_time(end_time);
        metric_timesteps += steps;

        return true;
    }

    bool Simulator::update()
    {
        if(net == nullptr)
            return false;

        for(auto &&elm : net->elements)
            refresh_neuron(elm.second);

        return true;
    }

    uint64_t Simulator::get_time() const
    {
        return net_time;
    }

    void Simulator::pull_network(Network *n) const
    {
        (void) n; // suppress compiler warning
        n = net;
    }

    double Simulator::get_metric(const std::string &metric)
    {
        double m = 0;

        if(metric == "fire_count")
        {
            m = metric_fires;
            metric_fires = 0;
        }
        else if(metric == "accumulate_count")
        {
            m = metric_accumulates;
            metric_accumulates = 0;
        }
        else if(metric == "depress_count")
        {
            m = 0;
        }
        else if(metric == "potentiate_count")
        {
            m = 0;
        }
        else if(metric == "total_timesteps")
        {
            m = metric_timesteps;
            metric_timesteps = 0;
        }
        else
        {
            fmt::print(std::cerr, "Specified device metric '{}' is not implemented\n", metric);
        }

        return m;
    }

    void Simulator::reset()
    {
        net_time = 0;
        input_fires.clear();
        net->reset();
        thresh_check.clear();

        // clear fire tracking information
        for(auto &&c : fire_counts) c = 0;
        for(auto &&t : last_fire_times) t = -1;
        for(auto &&a : monitor_aftertime) a = -1;
        for(auto &&r : recorded_fires) r.clear();

        monitor_precise.clear();
        monitor_precise.resize(net->num_outputs(), false);

        for(auto &&f : fires)
            f.clear();
    }

    void Simulator::clear_activity()
    {
        net_time = 0;
        input_fires.clear();
        net->clear_activity();
        thresh_check.clear();

        // clear fire tracking information
        for(auto &&c : fire_counts) c = 0;
        for(auto &&t : last_fire_times) t = -1;
        for(auto &&r : recorded_fires) r.clear();

        //for(auto &&a : monitor_aftertime) a = -1;
        //monitor_precise.clear();
        //monitor_precise.resize(net->num_outputs(), false);

        for(auto &&f : fires)
            f.clear();
    }

    Simulator::Simulator()
    {
        // empty after deleting old code
    }
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
