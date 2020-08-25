#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "framework.hpp"
#include "processor.hpp"
#include "concurrentqueue.h"

#include <vector>
#include <thread>
#include <memory>
#include <chrono>

using namespace neuro;
namespace py = pybind11;

using moodycamel::ConcurrentQueue;

struct WorkerData
{
    WorkerData(std::vector<Network*>& networks_, const nlohmann::json &config_, int steps_) : 
        networks(networks_), processor_config(config_), num_steps(steps_)
    {
        //results.resize(networks.size());
    }

    ConcurrentQueue<size_t> queue;
    std::vector<Network*>& networks;
    std::vector<std::vector<Spike>> encoded_data;
    //std::vector<std::vector<int>> results;
    const nlohmann::json& processor_config;
    int * results; // 2-d array
    int num_steps;
};

void predict(const nlohmann::json &j, Network *net, std::vector<std::vector<Spike>>& spikes, int num_steps, int* ret)
{
    auto p = caspian::Processor(j);

    p.load_network(net);

    for(size_t sample = 0; sample < spikes.size(); sample++)
    {
        // Apply spikes and simulate
        p.apply_spikes(spikes[sample]);
        p.run(num_steps);

        // Gather results
        int idx = 0, cnt = 0;
        for(size_t oid = 0; oid < net->num_outputs(); oid++)
        {
            int c = p.output_count(oid);
            if(c > cnt)
            {
                idx = oid;
                cnt = c;
            }
        }
        ret[sample] = idx;

        // Clear before next sample
        p.clear_activity();
    }
}

void pool_worker(WorkerData *info)
{
    size_t id;
    while(info->queue.try_dequeue(id))
    {
        predict(info->processor_config, 
                info->networks[id], 
                info->encoded_data, 
                info->num_steps, 
                &(info->results[id * info->encoded_data.size()]));
    }
}

//std::vector<std::vector<int>>
py::array_t<int> predict_all_pool(const nlohmann::json &j, EncoderArray *encoder,
        std::vector<Network*> networks, py::array_t<double>data, int num_steps, int num_threads)
{
    auto info = std::make_unique<WorkerData>(networks, j, num_steps);


    //auto t_start = std::chrono::system_clock::now();

    // Encode into spikes
    auto d = data.unchecked<2>();

    std::vector<double> dp(d.shape(1));
    for(size_t i = 0; i < d.shape(0); i++)
    {
        for(size_t k = 0; k < d.shape(1); k++)
        {
            dp[k] = d(i, k);
        }

        info->encoded_data.push_back(encoder->get_spikes(dp));
    }

    //auto t_encode = std::chrono::system_clock::now();

    // Allocate results array
    const int results_size = networks.size() * info->encoded_data.size();
    info->results = new int[results_size];

    py::capsule free_when_done(info->results, [](void *f) {
        int *ptr = reinterpret_cast<int *>(f);
        delete[] ptr;
    });

    //auto t_res = std::chrono::system_clock::now();

    // Fill queue
    for(size_t i = 0; i < networks.size(); i++)
    {
        info->queue.enqueue(i);
    }

    // Launch worker threads
    std::vector<std::thread> threads;
    for(size_t i = 0; i < num_threads; i++)
    {
        threads.emplace_back(pool_worker, info.get());
    }

    // Wait for completion
    for(size_t i = 0; i < threads.size(); i++)
    {
        threads[i].join();
    }

    //auto t_eval = std::chrono::system_clock::now();

    //std::chrono::duration<double> total_time = t_eval - t_start;
    //std::chrono::duration<double> encode_time = t_encode - t_start;
    //std::chrono::duration<double> allocate_time = t_res - t_encode;
    //std::chrono::duration<double> eval_time = t_eval - t_res;
    //fmt::print(" [Timing Data] Total: {:5.2f} Encode: {:6.3f} Allocate: {:6.3f} Eval: {:6.3f}\n",
    //        total_time.count(), encode_time.count(), allocate_time.count(), eval_time.count());

    return py::array_t<int>(
        {networks.size(), info->encoded_data.size()}, // shape
        {sizeof(int) * info->encoded_data.size(), sizeof(int)}, // strides
        info->results, // data ptr
        free_when_done); // deallocator object
}


void bind_fast_infer(py::module &m)
{
    m.def("fast_predict_all", &predict_all_pool,
            py::arg("proc_config"), py::arg("encoder"), py::arg("networks"),
            py::arg("data"), py::arg("num_steps"), py::arg("num_threads") = 4);
}
