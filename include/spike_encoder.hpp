#pragma once
#include <vector>
#include <tuple>

namespace caspian
{
    enum class SpikeVariable
    {
        NumSpikes,
        Interval
    };

    struct SpikeEncoder
    {
        SpikeEncoder(int _n_spikes, int _interval, double _dmin, double _dmax, SpikeVariable sv_ = SpikeVariable::NumSpikes) : 
            n_spikes(_n_spikes), interval(_interval), dmin(_dmin), dmax(_dmax), sv(sv_) {};
    
        std::vector<std::tuple<int,int>> encode(double data);
    
        int n_spikes;
        int interval;
        double dmin;
        double dmax;
        SpikeVariable sv;
    };
}
