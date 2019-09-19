#pragma once
#include <stdint.h>
#include <limits>
#include <vector>
#include <array>
#include <cmath>
#include <chrono>
#include <string>

namespace caspian
{
    namespace constants
    {
        const std::string STEM = "caspian";

        /* Synaptic weight */
        const int16_t  MIN_WEIGHT = -127;
        const int16_t  MAX_WEIGHT = 127;

        /* Synaptic delay (in cycles) */
        const uint8_t  MIN_DELAY = 0;
        const uint8_t  MAX_DELAY = 15;
        const uint8_t  DEFAULT_MAX_DELAY = 15;

        /* Axon/Neuron delay (in cycles) */
        const uint8_t  MIN_AXON_DELAY = 0;
        const uint8_t  MAX_AXON_DELAY = 15;
        const uint8_t  DEFAULT_MAX_AXON_DELAY = 0;

        inline uint16_t next_pow_of_2(uint16_t v)
        {
            v--;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            v++;
            return v;
        }

        /* Change this if the delay is increased. */
        inline constexpr size_t delay_bucket(uint64_t t, uint16_t mask)
        {
            return t & mask;
        }

        /* Neuron firing threshold (LIF style neurons) */
        const int32_t  MIN_CHARGE    = -32768;
        const int32_t  MAX_CHARGE    = 32767;
        const int16_t  MIN_THRESHOLD = 0;
        const int16_t  MAX_THRESHOLD = 255;

        /* EONS will eventually change parameters as needed -- these are just defaults prior to mutations */
        const uint16_t DEFAULT_INPUT_THRESH = 0;
        const uint16_t DEFAULT_OUTPUT_THRESH = 0;
        const uint16_t DEFAULT_INPUT_REFRAC = 0;
        const uint16_t DEFAULT_OUTPUT_REFRAC = 0;

        /* Neuron Leak */
        const int8_t  MIN_LEAK = -1; // -1 = no leak
        const int8_t  MAX_LEAK = 4; // max_tau => 2^4 = 16

        /* -t/tau where t is [0, tau-1] -- must be updated if MAX_LEAK changes */
        const int COMP_BITS = 10;
        const std::array<int,16> LEAK_COMP = {
            512, //  0/16 0/8 0/4 0/2 0/1
            535, //  1/16
            558, //  2/16 1/8
            583, //  3/16
            609, //  4/16 2/8 1/4
            636, //  5/16
            664, //  6/16 3/8
            693, //  7/16
            724, //  8/16 4/8 2/4 1/2
            756, //  9/16
            790, // 10/16 5/8
            825, // 11/16
            861, // 12/16 6/8 3/4
            899, // 13/16
            939, // 14/16 7/8
            981, // 15/16
        };

        /* Input Resolution -- subject to change in CASPIAN */
        const int16_t  MAX_DEVICE_INPUT = MAX_THRESHOLD;
        const int16_t  DEVICE_INPUT_BITS = 8;

        /* https://stackoverflow.com/questions/17719674/c11-fast-constexpr-integer-powers */
        /* This is not necessary for powers of 2, but I include it for the potential utility of it for non-power-of-2 cases */
        constexpr int64_t ipow(int64_t base, int exp, int64_t result = 1) {
            return exp < 1 ? result : ipow(base*base, exp/2, (exp % 2) ? result*base : result);
        }

        /* Maximum allowable time value during network execution */
        const uint64_t MAX_TIME = std::numeric_limits<int64_t>::max();

        /* version of the CASPIAN serialization format */
        const double  FORMAT_VER = 0.4;

        /*** Note: These parameters are magically determined. That said, I cannot promise that these are good. (ported from DANNA2 -> CASPIAN ***/

        /* Relative weight of mutating different properties */
        const double REL_WEIGHT_THRESHOLD = 100;
        const double REL_WEIGHT_REFRAC = 50;
        const double REL_WEIGHT_SYN_WEIGHT = 100;
        const double REL_WEIGHT_DELAY = 75;
        const double REL_WEIGHT_LEAK = 75;

        /* Proportion of elements which are changed when the property is mutated */
        const double REL_CHANGE_THRESHOLD = 0.2;
        const double REL_CHANGE_REFRAC = 0.15;
        const double REL_CHANGE_SYN_WEIGHT = 0.2;
        const double REL_CHANGE_DELAY = 0.2;
        const double REL_CHANGE_LEAK = 0.1;
    }
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
