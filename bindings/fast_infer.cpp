#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "framework.hpp"
#include "processor.hpp"
#include "concurrentqueue.h"

#include <vector>
#include <thread>

using namespace neuro;
namespace py = pybind11;

using moodycamel::ConcurrentQueue;

void predict(const nlohmann::json &j, Network *net, std::vector<std::vector<Spike>>& spikes, int num_steps, std::vector<int>& ret)
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
        ret.push_back(idx);

        // Clear before next sample
        p.clear_activity();
    }
}

void pool_worker(size_t thread_id, ConcurrentQueue<size_t>& work_queue, const nlohmann::json &j, std::vector<Network*> &networks, 
        std::vector<std::vector<Spike>>& spikes, int num_steps, std::vector<std::vector<int>> &ret)
{
    size_t id;

    while(true)
    {
        if(!work_queue.try_dequeue(id)) 
        {
            //fmt::print("[Thread {:3d}] Done.\n", thread_id);
            return;
        }
        //fmt::print("[Thread {:3d}] Predict {:4d}\n", thread_id, id);
        predict(j, networks[id], spikes, num_steps, ret[id]);
    }
}

std::vector<std::vector<int>> predict_all_pool(const nlohmann::json &j, EncoderArray *encoder, 
        std::vector<Network*> networks, py::array_t<double>data, int num_steps, int num_threads)
{
    ConcurrentQueue<size_t> work_queue(networks.size());
    std::vector<std::vector<int>> ret(networks.size());

    // Encode into spikes
    std::vector<std::vector<Spike>> encoded;
    auto d = data.unchecked<2>();

    std::vector<double> dp(d.shape(1));
    for(size_t i = 0; i < d.shape(0); i++)
    {
        for(size_t k = 0; k < d.shape(1); k++)
        {
            dp[k] = d(i, k);
        }

        encoded.push_back(encoder->get_spikes(dp));
    }

    // Fill queue
    for(size_t i = 0; i < networks.size(); i++)
    {
        work_queue.enqueue(i);
    }

    // Launch worker threads
    std::vector<std::thread> threads;
    for(size_t i = 0; i < num_threads; i++)
    {
        threads.emplace_back(pool_worker, i, std::ref(work_queue), std::ref(j), std::ref(networks), std::ref(encoded), num_steps, std::ref(ret));
    }

    // Wait for completion 
    for(size_t i = 0; i < threads.size(); i++)
    {
        threads[i].join();
    }

    //fmt::print(">>>All Threads Joined.<<<\n");

    return ret;
}

void bind_fast_infer(py::module &m)
{
    m.def("fast_predict_all", &predict_all_pool,
            py::arg("proc_config"), py::arg("encoder"), py::arg("networks"), 
            py::arg("data"), py::arg("num_steps"), py::arg("num_threads") = 4);
}
