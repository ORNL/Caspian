#pragma once
#include <array>
#include <vector>
#include <utility>
#include <queue>
#include <fstream>
#include <memory>
#include <libftdi1/ftdi.h>
#include "fmt/format.h"

#ifdef WITH_VERILATOR
#include "Vucaspian.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include "fifo.hpp"
#endif

#include "network.hpp"
#include "backend.hpp"
#include "simulator.hpp"
#include "constants.hpp"

namespace caspian
{

    /* uCaspian with UART Serial communication (ice40UP5k + FTDI FT232h) */
    class UsbCaspian : public Backend
    {
    private:
        //int send_cmd(uint8_t *buf, int size);
        //int rec_cmd(uint8_t *buf, int size);

        int send_cmd(const std::vector<uint8_t> &buf);
        std::vector<uint8_t> rec_cmd(int max_size);

        struct ftdi_context *ftdi;

    protected:
        //virtual void send_and_read(uint8_t *buf, int size, std::function<bool(void)> &&cond_func);
        virtual void send_and_read(std::vector<uint8_t> &buf, std::function<bool(void)> &&cond_func);

        /* process output */
        //int parse_cmds(uint8_t *buf, int size);
        int parse_cmds(const std::vector<uint8_t> &buf);
        int parse_cmd(const uint8_t *buf, int rem);

        template<class ...Args>
        void debug_print(Args&&... args) 
        { 
            if(m_debug) fmt::print(std::forward<Args>(args)...);
        }

        /* stores the currently loaded network */
        Network *net;

        /* id -> element coordinates for inputs */
        std::vector<uint32_t> input_map;

        std::vector<uint8_t> rec_leftover;

        /* output monitoring */
        std::vector<int64_t> monitor_aftertime;
        std::vector<bool> monitor_precise;
        std::vector<OutputMonitor> output_logs;

        /* queued fires */
        std::vector<InputFireEvent> input_fires;

        /* meta information */
        bool m_debug = false;
        unsigned int clr_acks = 0;
        unsigned int cfg_acks = 0;
        std::vector<std::pair<uint8_t,uint8_t>> rec_metrics;

        /* Network state */
        uint64_t run_start_time = 0;
        uint64_t exp_end_time;
        uint64_t net_time = 0;

    public:
        UsbCaspian(bool debug=false, std::string device = "");
        ~UsbCaspian();

        /* Queue fires into the array */
        void apply_input(int input_id, int16_t w, uint64_t t);

        /* Set the network to execute */
        bool configure(Network *network);
        bool configure_multi(std::vector<Network*>& networks);

        /* Simulate the network on the array for the specified timesteps */
        bool simulate(uint64_t steps);
        bool update();

        /* Get device metrics */
        double get_metric(const std::string &metric);

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
    };

#ifdef WITH_VERILATOR
    class VerilatorCaspian : public UsbCaspian
    {
    private:
        //int rec_cmd(uint8_t *buf, int size);
        std::vector<uint8_t> rec_cmd(int max_size);
        void step_sim(int clocks);

    protected: 
        //virtual void send_and_read(uint8_t *buf, int size, std::function<bool(void)> &&cond_func);
        virtual void send_and_read(std::vector<uint8_t> &buf, std::function<bool(void)> &&cond_func);

        /* Verilator objects */
        std::unique_ptr<Vucaspian> impl;
        std::unique_ptr<VerilatedFstC> fst;

        /* I/O fifos */
        std::unique_ptr<ByteFifo> fifo_in;
        std::unique_ptr<ByteFifo> fifo_out;

        uint64_t global_steps = 0;

        /* Logging information */
        bool m_logging = false;
        bool m_trace = false;
        std::string m_trace_file;

    public:
        VerilatorCaspian(bool debug, const std::string &trace = "");
        ~VerilatorCaspian();
    };
#endif
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
