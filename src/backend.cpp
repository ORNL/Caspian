#include "backend.hpp"

namespace caspian
{
    bool Backend::track_aftertime(uint32_t output_id, uint64_t aftertime)
    {
        if(output_id >= monitor_aftertime.size()) return false;
        monitor_aftertime[output_id] = aftertime;
        return true;
    }
    
    bool Backend::track_timing(uint32_t output_id, bool do_tracking)
    {
        if(output_id >= monitor_precise.size()) return false;
        monitor_precise[output_id] = do_tracking;
        return true;
    }

    int Backend::get_output_count(uint32_t output_id)
    {
        if(output_id >= fire_counts.size()) return -1;
        return fire_counts[output_id];
    }

    int Backend::get_last_output_time(uint32_t output_id)
    {
        if(output_id >= last_fire_times.size()) return -1;
        return last_fire_times[output_id];
    }

    std::vector<uint32_t> Backend::get_output_values(uint32_t output_id)
    {
        if(output_id >= recorded_fires.size()) return std::vector<uint32_t>();
        return recorded_fires.at(output_id);
    }
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
