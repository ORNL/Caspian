#pragma once
#include <array>
#include <vector>
#include <utility>
#include <queue>
#include <fstream>
#include <memory>

#ifdef WITH_VERILATOR
#include "Vucaspian.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include "fifo.hpp"
#endif

#include "network.hpp"
#include "backend.hpp"
#include "constants.hpp"

namespace caspian
{

    /* uCaspian with UART Serial communication (ice40UP5k + FTDI FT232h) */
    class UsbCaspian : public Backend
    {
    private:
        /* serial send/rec */
        int serial_open(const char *device, int rate);
        int send_cmd(uint8_t *buf, int size);
        int rec_cmd(uint8_t *buf, int size);

        /* Serial information */
        int serial_fd;
        int serial_rate;
        std::string serial_dev;

    protected:
        virtual void send_and_read(uint8_t *buf, int size, std::function<bool(void)> &&cond_func);

        /* process output */
        int parse_cmds(uint8_t *buf, int size);
        int parse_cmd(uint8_t *buf, int rem);

        /* queued fires */
        std::vector<InputFireEvent> input_fires;

        /* meta information */
        bool m_debug = false;
        unsigned int clr_acks = 0;
        unsigned int cfg_acks = 0;
        unsigned int lst_metric_addr = 0;
        unsigned int lst_metric_value = 0;

        /* Network state */
        uint64_t run_start_time = 0;
        uint64_t exp_end_time;
        uint64_t net_time = 0;

    public:
        UsbCaspian(const std::string &dev = "/dev/ttyUSB0", int rate = 3000000, bool debug=false);
        ~UsbCaspian();

        /* Queue fires into the array */
        void apply_input(int input_id, int16_t w, uint64_t t);

        /* Set the network to execute */
        bool configure(Network *network);

        /* Simulate the network on the array for the specified timesteps */
        bool simulate(uint64_t steps);
        bool update();

        /* Get device metrics */
        double get_metric(const std::string &metric);

        /* Get the current time */
        uint64_t get_time() const;

        /* pull the updated network -- has no meaning for Simulator -- network already has state */
        void pull_network(Network *net) const;

        /* Methods of resetting sim and network state */
        void reset();
        void clear_activity();
    };

#ifdef WITH_VERILATOR
    class VerilatorCaspian : public UsbCaspian
    {
    private:
        int rec_cmd(uint8_t *buf, int size);
        void step_sim(int clocks);

    protected: 
        virtual void send_and_read(uint8_t *buf, int size, std::function<bool(void)> &&cond_func);

        /* Verilator objects */
        std::unique_ptr<Vucaspian> impl;
        std::unique_ptr<VerilatedFstC> fst;

        /* I/O fifos */
        std::unique_ptr<ByteFifo> fifo_in;
        std::unique_ptr<ByteFifo> fifo_out;

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
