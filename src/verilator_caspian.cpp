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

    /*
    int VerilatorCaspian::rec_cmd(uint8_t *buf, int size)
    {
        int rec = 0;
        for(uint8_t *buf_p = buf; buf_p < buf+size && !fifo_out->empty(); buf_p++) {
            *buf_p = fifo_out->pop();
            rec++;
        }
        return rec;
    }
    */

    std::vector<uint8_t> VerilatorCaspian::rec_cmd(int max_size)
    {
        std::vector<uint8_t> ret;

        while(!fifo_out->empty() && ret.size() < max_size)
        {
            ret.push_back(fifo_out->pop());
        }

        return ret;
    }

    void VerilatorCaspian::send_and_read(std::vector<uint8_t> &buf, std::function<bool(void)> &&cond_func)
    {
        fifo_in->push_vec(buf);

        do
        {
            step_sim(1000);
            std::vector<uint8_t> rec = rec_cmd(4096);
            rec_leftover.insert(rec_leftover.end(), rec.begin(), rec.end());

            int processed = parse_cmds(rec_leftover);

            debug_print("[TIME: {}] Processed {} bytes ", net_time, processed);
            
            if(processed == rec_leftover.size())
            {
                rec_leftover.clear();
            }
            else
            {
                std::vector<uint8_t> new_leftover(rec_leftover.begin()+processed, rec_leftover.end());
                rec_leftover = std::move(new_leftover);
            }

            debug_print(" - {} leftover\n", rec_leftover.size());
        }
        while(!cond_func());
    }

    /*
    void VerilatorCaspian::send_and_read(uint8_t *buf, int size, std::function<bool(void)> &&cond_func)
    {
        const int rec_buf_sz = 4096;
        static uint8_t rec_buf[rec_buf_sz];
        static int rec_offset = 0;
        int parsed_bytes = 0;
        int rec_bytes = 0;
        int exp_proc_bytes = 0;

        // send commands by pushing to fifo
        // TODO
        for(int byte = 0; byte < size; byte++)
        {
            debug_print(" > PUSH {:2x}", buf[byte]);
            fifo_in->push(buf[byte]);
            debug_print(" -- OK\n");
        }
        debug_print("fifo_in: {} fifo_out: {}\n", fifo_in->size(), fifo_out->size());

        int tries = 0;

        do {
            // step verilator forward
            // TODO: how many clocks should it do?
            step_sim(1000);

            // zero buffer
            memset(rec_buf + rec_offset, 0, rec_buf_sz - rec_offset);

            // get output
            rec_bytes = rec_cmd(rec_buf + rec_offset, rec_buf_sz - rec_offset);

            // parse the buffer
            exp_proc_bytes = rec_bytes + rec_offset;
            parsed_bytes = parse_cmds(rec_buf, exp_proc_bytes);
            
            // determine if there are leftover bits to keep for next parse
            if(parsed_bytes != exp_proc_bytes)
            {
                rec_offset = exp_proc_bytes - parsed_bytes;
                memcpy(rec_buf, rec_buf + parsed_bytes, rec_offset);
            }
            else
            {
                rec_offset = 0;
            }

            if(m_debug)
            {
                fmt::print("[TIME: {}] Read {} bytes, Process {} bytes, offset {}\n", net_time, rec_bytes, parsed_bytes, rec_offset); 
                fmt::print("fifo_in: {} fifo_out: {}\n", fifo_in->size(), fifo_out->size());
                tries++;
                if(tries > 32)
                {
                    if(m_trace) fst->close();
                    throw std::runtime_error(" > No data output from the processor");
                }
            }

        } while(!cond_func());
    }
    */

}

#endif
