#include "spike_encoder.hpp"

namespace caspian
{
    std::vector<std::tuple<int,int>> SpikeEncoder::encode(double data)
    {
        double norm = 0;
        unsigned e_spikes = n_spikes;
        unsigned e_interval = interval;
        
        if (dmax != dmin)
            norm = (data - dmin) / (dmax - dmin);
    
        if(sv == SpikeVariable::NumSpikes)
        {
            e_spikes = n_spikes * std::min(1.0, std::max(0.0, norm));
            e_interval = interval;
        }
        else if(sv == SpikeVariable::Interval)
        {
            e_spikes = n_spikes;
            e_interval = interval * (1.0 - std::min(1.0, std::max(0.0, norm)));
        }
    
        std::vector<std::tuple<int,int>> ret(e_spikes);
    
        for(size_t i = 0; i < e_spikes; ++i)
            ret[i] = std::make_tuple(1, e_interval * i);
    
        return ret;
    }
}
