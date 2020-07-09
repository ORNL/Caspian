#pragma once

#include "network.hpp"
#include "backend.hpp"
#include "constants.hpp"

#include <vector>
#include <function>

namespace caspian
{
    using PktStream = std::vector<uint8_t>;
    using PktPtr = uint8_t*;

    class PacketFormat;
    class CommInterface;
    class Hardware;

    /* Handles different packet formats */
    class PacketFormat
    {
    public:
        using Condition = std::function<bool(State*)>;

        struct NeuronDesc
        {
            uint32_t id;
            uint32_t syn_start;
            uint32_t syn_cnt;
            uint16_t threshold;
            uint8_t  delay;
            int8_t   leak;
            bool     output;
        };

        struct SynapseDesc
        {
            uint32_t addr;
            uint32_t target;
            int16_t  weight;
            uint8_t  delay;
        };

        virtual void step(PktStream& buf, uint64_t steps) = 0;
        virtual void input(PktStream& buf, uint32_t id, uint8_t val) = 0;

        virtual void metric(PktStream& buf, uint8_t addr) = 0;

        virtual void clear_activity(PktStream& buf) = 0;
        virtual void clear_config(PktStream& buf) = 0;

        virtual void config_neuron(PktStream& buf, const NeuronDesc& n) = 0;
        virtual void config_synapse(PktStream& buf, const SynapseDesc& s) = 0;
        virtual void config_synapses(PktStream& buf, const std::vector<SynapseDesc>& s) = 0;

        virtual int  parse_cmd(Hardware& hw, const PktPtr buf, int rem) = 0;
        virtual int  parse_cmds(Hardware& hw, const PktStream& buf, Condition& cond) = 0;
    };

    class MicroPackets : public PacketFormat
    {
    public:
        MicroPackets(); // TODO: accept device configuration

        void step(PktStream& buf, uint64_t steps);
        void input(PktStream& buf, uint32_t id, uint8_t val);
        void metric(PktStream& buf, uint8_t addr);
        void clear_activity(PktStream& buf);
        void clear_config(PktStream& buf);
        void config_neuron(PktStream& buf, const NeuronDesc& n);
        void config_synapse(PktStream& buf, const SynapseDesc& s);
        void config_synapses(PktStream& buf, const std::vector<SynapseDesc>& s);

        int  parse_cmd(Hardware& hw, const PktPtr buf, int rem);
        int  parse_cmds(Hardware& hw, const PktStream& buf, Condition& cond);
    };

    /* Handles different communication interfaces (USB, PCIe) */
    class CommInterface
    {
    public:
        using condition = std::function<bool(State*)>;
        virtual void transaction(PktStream& buf, condition &&func) = 0;
    };

    class FtdiUSB : public CommInterface
    {
    public:
        FtdiUSB(); // TODO: accept device configuration
        void transaction(PktStream& buf, condition&& func);
    };

    /* uCaspian Verilator interface */
    class MicroVerilator : public CommInterface
    {
    public:
        MicroVerilator();
        void transaction(PktStream& buf, condition&& func);
    };

    class Hardware : public Backend
    {
    public:
        Hardware();

        /* Queue fires into the array */
        void apply_input(int input_id, int16_t w, uint64_t t);

        /* Set the network to execute */
        bool configure(Network *network);
        bool configure_multi(std::vector<Network*>& networks);

        /* Simulate the network on the array for the specified timesteps */
        bool simulate(uint64_t steps);
        bool update();

        /* Get device metrics */
        double get_metric(const std::string& metric);

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

        void set_debug(bool debug);

        void collect_all_spikes(bool collect = true); 
        std::vector<std::vector<uint32_t>> get_all_spikes();
        UIntMap get_all_spike_cnts();
    };

}
