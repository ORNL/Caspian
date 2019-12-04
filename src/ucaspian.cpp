#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <err.h>
#include <sys/ioctl.h>

#include "fmt/format.h"
#include "fmt/ostream.h"
#include "ucaspian.hpp"
#include "network.hpp"
#include "constants.hpp"


/* serial functions */
#ifdef NOT_DEFINED
static int rate_to_constant(int baudrate) 
{
#define B(x) case x: return B##x
    switch(baudrate) {
        B(50);     B(75);     B(110);    B(134);    B(150);
        B(200);    B(300);    B(600);    B(1200);   B(1800);
        B(2400);   B(4800);   B(9600);   B(19200);  B(38400);
        B(57600);  B(115200); B(230400); B(460800); B(500000); 
        B(576000); B(921600); B(1000000);B(1152000);B(1500000);
        default: return 0;
    }
#undef B
}
#endif

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
    
    inline void print_hex(uint8_t *buf, int size)
    {
        for(uint8_t *bp = buf; bp < buf + size; bp += 2)
        {
            printf("%02x%02x ", bp[0], bp[1]);
        }
        printf("\n");
    }

    inline void make_input_fire(uint8_t *buf, uint8_t id, uint8_t val)
    {
        buf[0] = (1 << 7) | id;
        buf[1] = val;
    }

    inline void make_step(uint8_t *buf, uint8_t steps)
    {
        buf[0] = 1;
        buf[1] = steps;
    }

    inline void make_get_metric(uint8_t *buf, uint8_t addr)
    {
        buf[0] = 2;
        buf[1] = addr;
    }

    inline void make_clear_activity(uint8_t *buf)
    {
        buf[0] = 4;
    }

    inline void make_clear_config(uint8_t *buf)
    {
        buf[0] = 8;
    }

    inline void make_cfg_neuron(uint8_t *buf, uint8_t addr, uint8_t threshold, 
            uint8_t delay, int8_t leak, bool output, uint16_t syn_start, uint8_t syn_cnt)
    {
        uint8_t enc_leak = leak + 1;
        uint8_t out_flag = (output) ? (1 << 3) : 0;

        buf[0] = 16;
        buf[1] = addr;
        buf[2] = threshold;
        buf[3] = ((delay & 0x0F) << 4) | out_flag | (enc_leak);
        buf[4] = (syn_start >> 8) & 0x0F;
        buf[5] = (syn_start & 0x00FF);
        buf[6] = syn_cnt;
    }

    inline void make_cfg_synapse(uint8_t *buf, uint16_t addr, int8_t weight, uint8_t target)
    {
        buf[0] = 32;
        buf[1] = (addr >> 8) & 0x0F;
        buf[2] = (addr & 0x00FF);
        buf[3] = weight;
        buf[4] = target;
    }

    UsbCaspian::UsbCaspian(const std::string &dev, int rate, bool debug)
    {
        serial_dev = dev;
        serial_rate = rate;
        m_debug = debug;

        if(dev == "verilator") return;

        // Open serial link
        serial_fd = serial_open(serial_dev.c_str(), serial_rate);

        if(serial_fd < 0)
        {
            throw std::runtime_error("Cannot open serial device");
        }
    }

    UsbCaspian::~UsbCaspian()
    {
        close(serial_fd);
    }

    int UsbCaspian::serial_open(const char *device, int /*rate*/)
    {
        struct termios options;
        int fd;

        /* Open and configure serial port */
        if ((fd = open(device, O_RDWR | O_NOCTTY)) == -1)
            return -1;

        fcntl(fd, F_SETFL, 0);

        memset(&options, 0, sizeof(struct termios));
        tcgetattr(fd, &options);
        cfmakeraw(&options);

        options.c_cflag |= (CLOCAL | CREAD);
        options.c_cflag |= CRTSCTS;

        if (tcsetattr(fd, TCSANOW, &options) != 0)
            return -1;

        return fd;
    }

    int UsbCaspian::send_cmd(uint8_t *buf, int size)
    {
        return write(serial_fd, buf, size);
    }

    int UsbCaspian::rec_cmd(uint8_t *buf, int size)
    {
        return read(serial_fd, buf, size);
    }

    int UsbCaspian::parse_cmds(uint8_t *buf, int size)
    {
        uint8_t *bufp;
        int incr = 1;
        int tincr = 0;
        int rem = size;

        for(bufp = buf; bufp < buf + size; bufp += incr)
        {
            rem = size - tincr;
            incr = parse_cmd(bufp, rem);
            if(incr == 0) break;
            tincr += incr;
        }

        return tincr;
    }

    int UsbCaspian::parse_cmd(uint8_t *buf, int rem)
    {
        int incr = 1;
        int addr;
        uint32_t id;
        uint64_t t = 0;
        int64_t time_diff;
        bool after_start;

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

                lst_metric_addr = buf[1];
                lst_metric_value = buf[2];
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

                if(m_debug) fmt::print("[t={}] Fire at {} -- output id {}\n", net_time, addr, id);

                if(after_start)
                {
                    fire_counts[id] += 1;
                    last_fire_times[id] = time_diff;

                    if(monitor_precise[id])
                        recorded_fires[id].push_back(time_diff);
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
        const int cfg_buf_sz = 4096;
        uint8_t cfg_buf[cfg_buf_sz];
        uint8_t *b_ptr = cfg_buf;
        int syn_cnt = 0;

        // clear configuration
        make_clear_config(cfg_buf);
        make_clear_activity(cfg_buf + sizeof_clear());
        send_and_read(cfg_buf, 2 * sizeof_clear(), [this](){ return clr_acks > 1; });

        // clear fire tracking information
        fire_counts.clear();
        last_fire_times.clear();
        monitor_aftertime.clear();
        monitor_precise.clear();
        recorded_fires.clear();

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

        fire_counts.resize(net->num_outputs(), 0);
        last_fire_times.resize(net->num_outputs(), -1);
        monitor_aftertime.resize(net->num_outputs(), -1);
        monitor_precise.resize(net->num_outputs(), false);
        recorded_fires.resize(net->num_outputs());

        if(m_debug)
        {
            fmt::print("[configure] outputs: {} fire_counts: {} last_fire_times: {}", 
                    net->num_outputs(), fire_counts.size(), last_fire_times.size());
            fmt::print(" monitor_aftertime: {} monitor_precise: {} recorded_fires {}\n", 
                    monitor_aftertime.size(), monitor_precise.size(), recorded_fires.size());
        }

        net_time = 0;
        cfg_acks = 0;
        clr_acks = 0;

        // Generate configuration packets
        // TODO: handle overflow in buffer
        for(auto &&elm : (*net))
        {
            const Neuron *n = elm.second;

            // add neuron
            int n_syn_start = syn_cnt;
            int n_syn_cnt = n->outputs.size();
            bool output_en = (n->output_id >= 0);
            make_cfg_neuron(b_ptr, n->id, n->threshold, n->delay, n->leak, output_en, n_syn_start, n_syn_cnt);
            b_ptr += sizeof_cfg_neuron();

            // add synapses
            for(const std::pair<Neuron*, Synapse*> &p : n->outputs)
            {
                make_cfg_synapse(b_ptr, syn_cnt, p.second->weight, p.first->id);
                syn_cnt++;
                b_ptr += sizeof_cfg_synapse();
            }
        }

        send_and_read(cfg_buf, b_ptr - cfg_buf, [this](){ return cfg_acks >= (net->num_neurons() + net->num_synapses()); });

        return true;
    }

    bool UsbCaspian::configure_multi(std::vector<Network*>& /* networks */)
    {
        throw std::logic_error("Configure Multi is not implemented for uCaspian (yet...)");
        return false;
    }

    bool UsbCaspian::simulate(uint64_t steps)
    {
        const int send_buf_sz = 1024;

        uint64_t end_time = net_time + steps;
        uint8_t send_buf[send_buf_sz];
        uint8_t *buf_p = send_buf;
        uint64_t cur_time = net_time;

        exp_end_time = end_time;

        memset(send_buf, 0, send_buf_sz);

        auto make_steps = [&](int steps){
            int step_size = 0;
            do
            {
                step_size = (steps > 255) ? 255 : steps;
                steps -= step_size;
                make_step(buf_p, step_size);
                buf_p += sizeof_step();
                cur_time += step_size;
            } while(steps > 0);
        };

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

            make_input_fire(buf_p, net->get_input(f.id), f.weight);
            buf_p += sizeof_input_fire();
        }

        if(cur_time < end_time)
        {
            make_steps(end_time - cur_time);
        }

        send_and_read(send_buf, buf_p - send_buf, [this](){ return net_time >= exp_end_time; });

        return true;
    }

    void UsbCaspian::send_and_read(uint8_t *buf, int size, std::function<bool(void)> &&cond_func)
    {
        const int rec_buf_sz = 4096;
        static uint8_t rec_buf[rec_buf_sz];
        static int rec_offset = 0;
        int wr_bytes = 0;
        int processed = 0;
        int rec_bytes = 0;
        int exp_proc_bytes = 0;

        // send commands
        if(size > 0)
        {
            if(m_debug) fmt::print("Attempt send of {} bytes\n", size);
            wr_bytes = send_cmd(buf, size);
            if(m_debug) fmt::print("Sent {} bytes\n", wr_bytes);

            if(size != wr_bytes)
            {
                fmt::print("Incomplete write - actual: {} expected: {}\n", size, wr_bytes);
            }
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

            if(m_debug) fmt::print("[TIME: {}] Read {} bytes, Process {} bytes, offset {}\n", net_time, rec_bytes, processed, rec_offset);

        } while(!cond_func());

    }

    uint64_t UsbCaspian::get_time() const
    {
        return net_time;
    }

    double UsbCaspian::get_metric(const std::string & /*metric*/)
    {
        // TODO
        return 0;
    }

    void UsbCaspian::reset()
    {
        uint8_t send_buf[sizeof_clear()];
        make_clear_activity(send_buf);
        clr_acks = 0;

        send_and_read(send_buf, sizeof_clear(), [this](){ return clr_acks > 0; });
        
        net_time = 0;
        input_fires.clear();
        clr_acks = 0;

        // clear fire tracking information
        for(auto &c : fire_counts) c = 0;
        for(auto &t : last_fire_times) t = -1;
        for(auto &a : monitor_aftertime) a = -1;
        for(auto &r : recorded_fires) r.clear();
        for(auto &&p : monitor_precise) p = false;
    }

    void UsbCaspian::clear_activity()
    {
        uint8_t send_buf[sizeof_clear()];
        make_clear_activity(send_buf);
        clr_acks = 0;

        send_and_read(send_buf, sizeof_clear(), [this](){ return clr_acks > 0; });

        net_time = 0;
        clr_acks = 0;
        input_fires.clear();

        // clear fire tracking informy8yation
        for(auto &c : fire_counts) c = 0;
        for(auto &t : last_fire_times) t = -1;
        for(auto &r : recorded_fires) r.clear();
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

    int UsbCaspian::get_output_count(uint32_t output_id, int /*network_id*/)
    {
        if(output_id >= fire_counts.size()) return -1;
        return fire_counts[output_id];
    }

    int UsbCaspian::get_last_output_time(uint32_t output_id, int /*network_id*/)
    {
        if(output_id >= last_fire_times.size()) return -1;
        return last_fire_times[output_id];
    }

    std::vector<uint32_t> UsbCaspian::get_output_values(uint32_t output_id, int /*network_id*/)
    {
        if(output_id >= recorded_fires.size()) return std::vector<uint32_t>();
        return recorded_fires.at(output_id);
    }


}

#undef CFG_ACK
#undef CLR_ACK
#undef METRIC_RESP
#undef TIME_UPDATE
#undef OUTPUT_FIRE
