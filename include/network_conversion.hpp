#pragma once
#include "framework.hpp"
#include "network.hpp"

namespace caspian
{
    bool network_framework_to_internal(neuro::Network *tn, caspian::Network *net);
    bool network_internal_to_framework(caspian::Network *net, neuro::Network *tn);
}
