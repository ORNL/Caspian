#ifdef WITH_VERILATOR
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <memory>

#include "fmt/format.h"
#include "fmt/ostream.h"
#include "ucaspian.hpp"
#include "network.hpp"
#include "constants.hpp"


namespace caspian
{
    /* A simple C++11 implementation of make_unique
     * Why did C++11 have make_shared but not make_unique? Perhaps we'll never know. */
    template<typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    VerilatorCaspian::VerilatorCaspian(bool debug, const std::string &trace) : UsbCaspian(debug, "verilator")
    {
        impl = make_unique<Vucaspian>();

        if(!trace.empty())
        {
            m_trace = true;
            m_trace_file = trace;

            // create trace object
            Verilated::traceEverOn(true);
            fst = make_unique<VerilatedFstC>();
            impl->trace(fst.get(), 99);
            fst->open(trace.c_str());
        }

        // init ports
        impl->sys_clk = 1;
        impl->reset = 1;

        // create i/o fifo structures
        fifo_in = make_unique<ByteFifo>(
                &(impl->sys_clk),
                &(impl->read_rdy),
                &(impl->read_vld),
                &(impl->read_data),
                true,
                false
            );

        fifo_out = make_unique<ByteFifo>(
                &(impl->sys_clk),
                &(impl->write_rdy),
                &(impl->write_vld),
                &(impl->write_data),
                false,
                false
            );

        // finish reset sequence
        step_sim(1);
        impl->reset = 0;
        step_sim(1);

        global_steps = 0;
    }

    VerilatorCaspian::~VerilatorCaspian()
    {
        // close trace file
        if(m_trace)
        {
            fst->close();
        }
    }

    void VerilatorCaspian::step_sim(int clocks)
    {
        uint64_t stop_step = global_steps + clocks;

        while(!Verilated::gotFinish())
        {
            for(int c = 0; c < 2; ++c)
            {
                if(m_trace) fst->dump(2*global_steps+c);

                // toggle the clock
                impl->sys_clk = !impl->sys_clk;

                fifo_in->eval(impl->sys_clk, impl->reset);
                impl->eval();
                fifo_out->eval(impl->sys_clk, impl->reset);
            }

            // increment step counter
            global_steps++;

            // check for stopping condition
            if(global_steps > stop_step) break;
        }
    }

    std::vector<uint8_t> VerilatorCaspian::rec_cmd(int max_size)
    {
        std::vector<uint8_t> ret;

        while(!fifo_out->empty() && ret.size() < max_size)
        {
            ret.push_back(fifo_out->pop());
        }

        return ret;
    }

    void VerilatorCaspian::send_and_read(std::vector<uint8_t> &buf, std::function<bool(HardwareState*)> &&cond_func)
    {
        unsigned nop_count = 0;
        fifo_in->push_vec(buf);

        do
        {
            step_sim(250);
            std::vector<uint8_t> rec = rec_cmd(4096);
            hw_state->rec_leftover.insert(hw_state->rec_leftover.end(), rec.begin(), rec.end());

            int processed = hw_state->parse_cmds_cond(hw_state->rec_leftover, cond_func);

            // keep track of if we get any data
            if(processed == 0) nop_count++;
            else nop_count = 0;

            // break out if we think things are dead
            if(nop_count > 1000) throw std::runtime_error("Simulation appears frozen");

            debug_print("[TIME: {}] Processed {} bytes ", hw_state->net_time, processed);
            
            if(processed == hw_state->rec_leftover.size())
            {
                hw_state->rec_leftover.clear();
            }
            else
            {
                std::vector<uint8_t> new_leftover(hw_state->rec_leftover.begin()+processed, hw_state->rec_leftover.end());
                hw_state->rec_leftover = std::move(new_leftover);
            }

            hw_state->debug_print(" - {} leftover\n", hw_state->rec_leftover.size());
        }
        while(!cond_func(hw_state.get()));
    }

}

#endif
