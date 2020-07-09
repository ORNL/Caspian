#include "hardware.hpp"

namespace caspian
{
    Hardware::Hardware()
    {
        // TODO
    }

    void Hardware::apply_input(int input_id, int16_t w, uint64_t t)
    {

    }

    bool Hardware::configure(Network *network)
    {

    }

    bool Hardware::configure_multi(std::vector<Network*>& networks)
    {

    }

    bool Hardware::simulate(uint64_t steps)
    {

    }
    
    bool Hardware::update()
    {

    }

    double Hardware::get_metric(const std::string& metric)
    {

    }

    uint64_t Hardware::get_time() const
    {

    }

    Network* Hardware::pull_network(uint32_t idx) const
    {

    }

    void Hardware::reset()
    {

    }

    void Hardware::clear_activity()
    {

    }

    bool Hardware::track_aftertime(uint32_t output_id, uint64_t aftertime)
    {

    }
    
    bool Hardware::track_timing(uint32_t output_id, bool do_tracking = true)
    {

    }

    int  Hardware::get_output_count(uint32_t output_id, int network_id = 0)
    {

    }

    int  Hardware::get_last_output_time(uint32_t output_id, int network_id = 0)
    {

    }

    std::vector<uint32_t> Hardware::get_output_values(uint32_t output_id, int network_id = 0)
    {

    }

    void Hardware::set_debug(bool debug)
    {

    }

    void Hardware::collect_all_spikes(bool collect = true)
    {

    }

    std::vector<std::vector<uint32_t>> Hardware::get_all_spikes()
    {

    }

    Hardware::UIntMap Hardware::get_all_spike_cnts()
    {

    }

}
