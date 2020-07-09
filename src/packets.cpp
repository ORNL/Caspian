#include "hardware.hpp"

namespace caspian
{
    MicroPackets::MicroPackets()
    {
        // TODO: configuration
    }

    void MicroPackets::step(PktStream& buf, uint64_t steps)
    {

    }

    void MicroPackets::input(PktStream& buf, uint32_t id, uint8_t val)
    {

    }

    void MicroPackets::metric(PktStream& buf, uint8_t addr)
    {

    }

    void MicroPackets::clear_activity(PktStream& buf)
    {

    }

    void MicroPackets::clear_config(PktStream& buf)
    {

    }

    void MicroPackets::config_neuron(PktStream& buf, const NeuronDesc& n)
    {

    }

    void MicroPackets::config_synapse(PktStream& buf, const SynapseDesc& s)
    {

    }

    void MicroPackets::config_synapses(PktStream& buf, const std::vector<SynapseDesc>& s)
    {

    }

    int  MicroPackets::parse_cmd(Hardware& hw, const PktPtr buf, int rem)
    {

    }

    int  MicroPackets::parse_cmds(Hardware& hw, const PktStream& buf, Condition& cond)
    {

    }

}
