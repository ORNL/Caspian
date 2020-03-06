#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>

#include "fmt/format.h"
#include "fmt/ostream.h"
#include "ucaspian.hpp"
#include "network.hpp"
#include "constants.hpp"

#define OUTPUT_FIRE (128)
#define CFG_ACK     (112)
#define CLR_ACK      (12)
#define METRIC_RESP   (2)
#define TIME_UPDATE   (1)

namespace caspian
{

    /* packet functions */
    inline constexpr int sizeof_input_fire()     { return 2; }
    inline constexpr int sizeof_step()           { return 2; }
    inline constexpr int sizeof_get_metric()     { return 2; }
    inline constexpr int sizeof_clear()          { return 1; }
    inline constexpr int sizeof_cfg_neuron()     { return 7; }
    inline constexpr int sizeof_cfg_synapse()    { return 5; }
    // TODO inline constexpr int sizeof_cfg_synapse_multi(int cnt) { return 5 + 2*cnt; }
    
    static const std::map<std::string, std::vector<uint8_t>> metric_addrs = {
        { "fire_count",         {1, 2} },
        { "accumulate_count",   {3, 4} },
        { "depress_count",      {} },
        { "potentiate_count",   {} },
        { "total_timesteps",    {} }
    };
    
    inline void print_hex(uint8_t *buf, int size)
    {
        for(uint8_t *bp = buf; bp < buf + size; bp += 2)
        {
            printf("%02x%02x ", bp[0], bp[1]);
        }
        printf("\n");
    }

    inline void make_input_fire(std::vector<uint8_t>& buf, uint8_t id, uint8_t val)
    {
        uint8_t op = (1 << 7) | id;
        buf.insert(buf.end(), {op, val});
    }

    inline void make_step(std::vector<uint8_t>& buf, uint8_t steps)
    {
        buf.insert(buf.end(), {1, steps});
    }

    inline void make_get_metric(std::vector<uint8_t>& buf, uint8_t addr)
    {
        buf.insert(buf.end(), {2, addr});
    }

    inline void make_clear_activity(std::vector<uint8_t>& buf)
    {
        buf.push_back(4);
    }

    inline void make_clear_config(std::vector<uint8_t>& buf)
    {
        buf.push_back(8);
    }

    inline void make_cfg_neuron(std::vector<uint8_t>& buf, uint8_t addr, uint8_t threshold, 
            uint8_t delay, int8_t leak, bool output, uint16_t syn_start, uint8_t syn_cnt)
    {
        uint8_t enc_leak = leak + 1;
        uint8_t out_flag = (output) ? (1 << 3) : 0;
        uint8_t dly_and_flg = ((delay & 0x0F) << 4) | out_flag | (enc_leak);
        uint8_t syn_0 = (syn_start >> 8) & 0x0F;
        uint8_t syn_1 = syn_start & 0x0F;

        buf.insert(buf.end(), {16, addr, threshold, dly_and_flg, syn_0, syn_1, syn_cnt});
    }

    inline void make_cfg_synapse(std::vector<uint8_t>& buf, uint16_t addr, int8_t weight, uint8_t target)
    {
        uint8_t addr_0 = (addr >> 8) & 0x0F;
        uint8_t addr_1 = addr & 0x00FF;

        buf.insert(buf.end(), {32, addr_0, addr_1, static_cast<uint8_t>(weight), target});
    }

    UsbCaspian::UsbCaspian(bool debug, std::string device)
    {
        int ret;

        set_debug(debug);

        // TODO: interpret device string
        if(device != "verilator")
        {
            // create ftdi context struct (c library)
            if((ftdi = ftdi_new()) == 0)
            {
                throw std::runtime_error("Could not create libftdi context");
            }

            // attempt to open the device with the given vendor/device id
            if((ret = ftdi_usb_open(ftdi, 0x0403, 0x6010)) < 0)
            {
                const char *ftdi_err = ftdi_get_error_string(ftdi);
                std::string err = fmt::format("libftdi usb open error: {}", ftdi_err); 
                ftdi_free(ftdi);
                throw std::runtime_error(err);
            }

            // set latency timer
            ftdi_set_latency_timer(ftdi, 1);
        }
        else
        {
            ftdi = nullptr;
        }
    }

    UsbCaspian::~UsbCaspian()
    {
        if(ftdi != nullptr) ftdi_free(ftdi);
    }

    /*
    int UsbCaspian::send_cmd(uint8_t *buf, int size)
    {
        return ftdi_write_data(ftdi, buf, size);
    }

    int UsbCaspian::rec_cmd(uint8_t *buf, int size)
    {
        return ftdi_read_data(ftdi, buf, size);
    }
    */

    int UsbCaspian::send_cmd(const std::vector<uint8_t> &buf)
    {
        return ftdi_write_data(ftdi, buf.data(), buf.size());
    }

    std::vector<uint8_t> UsbCaspian::rec_cmd(int max_size)
    {
        if(max_size > 8192) throw std::logic_error("Cannot get more than 8k in one command");

        uint8_t buf[8192];
        int bytes = ftdi_read_data(ftdi, buf, max_size);
        return std::vector<uint8_t>(buf, buf+bytes);
    }

    int UsbCaspian::parse_cmds(const std::vector<uint8_t> &buf)
    {
        debug_print("Enter parse_cmds -- buf size: {}\n", buf.size());
        int offset = 0;

        while(offset < buf.size())
        {
            int rem = buf.size() - offset;
            debug_print(" > Rem: {}", rem);
            int inc = parse_cmd(buf.data()+offset, rem);
            debug_print(" Inc: {}\n", inc);
            offset += inc;
            if(inc == 0) break;
        }

        return offset;
    }

    int UsbCaspian::parse_cmd(const uint8_t *buf, int rem)
    {
        int incr = 1;
        int addr;
        uint32_t id;
        uint64_t t = 0;
        int64_t time_diff;
        bool after_start;
        uint8_t metric_addr, metric_value;

        switch(buf[0])
        {
            case CFG_ACK:
                cfg_acks++;
                break;

            case CLR_ACK:
                clr_acks++;
                break;

            case METRIC_RESP:
                if(rem < 3)
                {
                    return 0;
                }

                metric_addr = buf[1];
                metric_value = buf[2];
                rec_metrics.emplace_back(metric_addr, metric_value);
                incr = 3;
                break;

            case TIME_UPDATE:
                if(rem < 5)
                {
                    return 0;
                }

                t = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
                incr = 5;

                if(t - net_time > 255)
                    fmt::print("Corrupted time {} -> {} -- {} {} {} {} {}\n", net_time, t, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

                // update time
                net_time = t;

                break;

            case OUTPUT_FIRE:

                if(rem < 2)
                {
                    return 0;
                }

                addr = buf[1];
                incr = 2;

                if(!net->is_neuron(addr))
                {
                    fmt::print("Corrupted fire {}\n", addr);
                    break;
                }

                id = net->get_neuron(addr).output_id;

                // add fire
                time_diff = net_time - run_start_time;
                after_start = (time_diff >= monitor_aftertime[id]);

                debug_print("[t={}] Fire at {} -- output id {}\n", net_time, addr, id);

                if(after_start)
                {
                    output_logs[0].add_fire(id, net_time - run_start_time, monitor_precise[id]);
                }
                
                break;

            default:
                // do nothing
                break;
        }
        
        return incr;
    }

    void UsbCaspian::apply_input(int input_id, int16_t w, uint64_t t)
    {
        input_fires.emplace_back(net->get_input(input_id), w, net_time + t);
    }

    bool UsbCaspian::configure(Network *new_net)
    {
        std::vector<uint8_t> cfg_buf;
        int syn_cnt = 0;

        // make some no-ops
        for(int i = 0; i < 32; i++) cfg_buf.push_back(0);

        // clear configuration
        make_clear_config(cfg_buf);
        make_clear_activity(cfg_buf);
        debug_print("Preparing to send clear config, clear activity...");
        send_and_read(cfg_buf, [this](){ return clr_acks > 1; });
        //send_and_read(cfg_buf.data(), cfg_buf.size(), [this](){ return clr_acks > 1; });
        debug_print(" Clear ack'd\n");
        cfg_buf.clear();

        // clear fire tracking information
        monitor_aftertime.clear();
        monitor_precise.clear();
        output_logs.clear();

        if(new_net == nullptr)
        {
            net = nullptr;
            return false;
        }
        else if(new_net->num_neurons() > 256 || new_net->num_synapses() > 4096)
        {
            fmt::print(std::cerr, 
                "Network is too large with {} neurons and {} synapses for the uCaspian device\n", 
                new_net->num_neurons(), new_net->num_synapses());

            net = nullptr;
            return false;
        }

        net = new_net;

        monitor_aftertime.resize(net->num_outputs(), -1);
        monitor_precise.resize(net->num_outputs(), false);
        output_logs.emplace_back(net->num_outputs());

        debug_print("[configure] outputs: {} ", net->num_outputs());
        debug_print(" monitor_aftertime: {} monitor_precise: {} output_logs {}\n", 
                monitor_aftertime.size(), monitor_precise.size(), output_logs.size());

        net_time = 0;
        cfg_acks = 0;
        clr_acks = 0;

        unsigned int elms_prog = 0;

        // Generate configuration packets
        // TODO: handle overflow in buffer
        for(auto &&elm : (*net))
        {
            const Neuron *n = elm.second;

            // add neuron
            int n_syn_start = syn_cnt;
            int n_syn_cnt = n->outputs.size();
            bool output_en = (n->output_id >= 0);
            make_cfg_neuron(cfg_buf, n->id, n->threshold, n->delay, n->leak, output_en, n_syn_start, n_syn_cnt);
            elms_prog++;

            // add synapses
            for(const std::pair<Neuron*, Synapse*> &p : n->outputs)
            {
                make_cfg_synapse(cfg_buf, syn_cnt, p.second->weight, p.first->id);
                syn_cnt++;
                elms_prog++;
            }
        }

        if(elms_prog > 0)
        {
            fmt::print("Send config for {} elements with {} bytes\n", elms_prog, cfg_buf.size()); 
            //send_and_read(cfg_buf.data(), cfg_buf.size(), [=](){ return cfg_acks >= elms_prog; });
            send_and_read(cfg_buf, [=](){ return cfg_acks >= elms_prog; });
        }

        return true;
    }

    bool UsbCaspian::configure_multi(std::vector<Network*>& /* networks */)
    {
        throw std::logic_error("Configure Multi is not implemented for uCaspian (yet...)");
        return false;
    }

    bool UsbCaspian::simulate(uint64_t steps)
    {
        std::vector<uint8_t> send_buf;
        uint64_t end_time = net_time + steps;
        uint64_t cur_time = net_time;

        run_start_time = net_time;
        exp_end_time = end_time;

        auto make_steps = [&](int steps){
            int step_size = 0;
            do
            {
                step_size = (steps > 255) ? 255 : steps;
                steps -= step_size;

                debug_print(" > STEP {}\n", step_size);

                make_step(send_buf, step_size);
                cur_time += step_size;
            } while(steps > 0);
        };

        // clear fire tracking information
        for(auto &m : output_logs) m.clear();

        // First, sort our input fires
        std::sort(input_fires.begin(), input_fires.end(), std::less<InputFireEvent>());

        // Queue fires
        for(auto &&f : input_fires)
        {
            // check if fire is within current time window
            if(f.time < cur_time || f.time > end_time)
            {
                continue;
            }
            else if(f.time > cur_time)
            {
                make_steps(f.time - cur_time);
            }

            make_input_fire(send_buf, f.id, f.weight);
            debug_print("[t={:3d}] FIRE {:3d}:{:3d}\n", cur_time, f.id, f.weight);
        }

        if(cur_time < end_time)
        {
            make_steps(end_time - cur_time);
        }

        if(send_buf.size() > 0)
        {
            //send_and_read(send_buf.data(), send_buf.size(), [this](){ return net_time >= exp_end_time; });
            send_and_read(send_buf, [this](){ return net_time >= exp_end_time; });
        }

        return true;
    }

    void UsbCaspian::send_and_read(std::vector<uint8_t> &buf, std::function<bool(void)> &&cond_func)
    {
        struct ftdi_transfer_control *send_req = ftdi_write_data_submit(ftdi, buf.data(), buf.size());

        if(send_req == NULL)
        {
            throw std::runtime_error("FTDI write failed.");
        }

        do
        {
            std::vector<uint8_t> rec = rec_cmd(4096);
            rec_leftover.insert(rec_leftover.end(), rec.begin(), rec.end());

            int processed = parse_cmds(rec_leftover);

            debug_print("[TIME: {}] Processed {} bytes ");
            
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

        // wait for send to complete -- which it probably has
        //while(ftdi_transfer_data_done(send_req) > 0); 
        //libusb_free_transfer(send_req->transfer);
        //free(send_req);
    }

    /*
    void UsbCaspian::send_and_read(uint8_t *buf, int size, std::function<bool(void)> &&cond_func)
    {
        const int rec_buf_sz = 1024;
        static uint8_t rec_buf[rec_buf_sz];
        static int rec_offset = 0;
        int processed = 0;
        int rec_bytes = 0;
        int exp_proc_bytes = 0;

        // async send
        struct ftdi_transfer_control *send_req = ftdi_write_data_submit(ftdi, buf, size);

        if(send_req == NULL)
        {
            throw std::runtime_error("FTDI write failed");
        }

        do {
            // zero buffer
            memset(rec_buf + rec_offset, 0, rec_buf_sz - rec_offset);

            // get output
            rec_bytes = rec_cmd(rec_buf + rec_offset, rec_buf_sz - rec_offset);

            // parse the buffer
            exp_proc_bytes = rec_bytes + rec_offset;
            processed = parse_cmds(rec_buf, exp_proc_bytes);
            
            // determine if there are leftover bits to keep for next parse
            if(processed != exp_proc_bytes)
            {
                rec_offset = (exp_proc_bytes - processed);
                memcpy(rec_buf, rec_buf + processed, rec_offset);
            }
            else
            {
                rec_offset = 0;
            }

            debug_print("[TIME: {}] Read {} bytes, Process {} bytes, offset {}\n", net_time, rec_bytes, processed, rec_offset);

        } while(!cond_func());
    }
    */

    uint64_t UsbCaspian::get_time() const
    {
        return net_time;
    }

    double UsbCaspian::get_metric(const std::string& metric)
    {
        auto mit = metric_addrs.find(metric);
        if(mit == metric_addrs.end() || mit->second.empty())
        {
            fmt::print("Metric '{}' is not implemented for uCaspian.\n", metric);
            return 0;
        }

        unsigned int metric_bytes = mit->second.size();
        std::vector<uint8_t> buf;
        rec_metrics.clear();

        for(auto addr = mit->second.begin(); addr != mit->second.end(); addr++)
        {
            fmt::print("Get metric addr: {}\n", *addr);
            make_get_metric(buf, *addr);
        }

        //send_and_read(buf.data(), buf.size(), [=](){ return rec_metrics.size() >= metric_bytes; });
        send_and_read(buf, [=](){ return rec_metrics.size() >= metric_bytes; });

        int64_t val = 0;

        for(auto metric : rec_metrics)
        {
            fmt::print("Addr: {} Value: {}\n", metric.first, metric.second);
            val = val << 8;
            val |= metric.second;
        }

        rec_metrics.clear();

        return val;
    }

    void UsbCaspian::reset()
    {
        std::vector<uint8_t> send_buf;
        make_clear_activity(send_buf);
        clr_acks = 0;

        //send_and_read(send_buf.data(), send_buf.size(), [this](){ return clr_acks > 0; });
        send_and_read(send_buf, [this](){ return clr_acks > 0; });
        
        net_time = 0;
        clr_acks = 0;
        input_fires.clear();

        // clear fire tracking information
        for(auto &a : monitor_aftertime) a = -1;
        for(auto &m : output_logs) m.clear();
        for(auto &&p : monitor_precise) p = false;
    }

    void UsbCaspian::clear_activity()
    {
        std::vector<uint8_t> send_buf;
        make_clear_activity(send_buf);
        clr_acks = 0;

        //send_and_read(send_buf.data(), send_buf.size(), [this](){ return clr_acks > 0; });
        send_and_read(send_buf, [this](){ return clr_acks > 0; });

        net_time = 0;
        clr_acks = 0;
        input_fires.clear();

        // clear fire tracking information
        for(auto &m : output_logs) m.clear();
    }

    bool UsbCaspian::update()
    {
        return true;
    }
    
    Network* UsbCaspian::pull_network(uint32_t /* idx */) const
    {
        // TODO
        return net;
    }

    bool UsbCaspian::track_aftertime(uint32_t output_id, uint64_t aftertime)
    {
        if(output_id >= monitor_aftertime.size()) return false;
        monitor_aftertime[output_id] = aftertime;
        return true;
    }
    
    bool UsbCaspian::track_timing(uint32_t output_id, bool do_tracking)
    {
        if(output_id >= monitor_precise.size()) return false;
        monitor_precise[output_id] = do_tracking;
        return true;
    }

    int UsbCaspian::get_output_count(uint32_t output_id, int network_id)
    {
        if(output_id >= output_logs[network_id].fire_counts.size()) return -1;
        return output_logs[network_id].fire_counts[output_id];
    }

    int UsbCaspian::get_last_output_time(uint32_t output_id, int network_id)
    {
        if(output_id >= output_logs[network_id].last_fire_times.size()) return -1;
        return output_logs[network_id].last_fire_times[output_id];
    }

    std::vector<uint32_t> UsbCaspian::get_output_values(uint32_t output_id, int network_id)
    {
        if(output_id >= output_logs[network_id].recorded_fires.size()) 
            return std::vector<uint32_t>();

        return output_logs[network_id].recorded_fires[output_id];
    }

    void UsbCaspian::set_debug(bool debug)
    {
        m_debug = debug;
    }

}

#undef CFG_ACK
#undef CLR_ACK
#undef METRIC_RESP
#undef TIME_UPDATE
#undef OUTPUT_FIRE
