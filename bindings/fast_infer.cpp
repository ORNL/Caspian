/**
 * Fast inference for basic classification tasks
 *
 * This implements functions and Python bindings to allow
 * fast batched inference for basic classification tasks.
 * A key advantage is the use of C++ worker threads and 
 * minimizing the amount of data copied. The speed up over
 * the typical Python way can easily be 3-5x.
 *
 * Note: This code currently is designed to only work with Caspian.
 *
 */


#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "framework.hpp"
#include "processor.hpp"
#include "concurrentqueue.h"

#include <vector>
#include <thread>
#include <memory>

using namespace neuro;
namespace py = pybind11;

using moodycamel::ConcurrentQueue;

struct WorkerData
{
    WorkerData(std::vector<Network*>& networks_, const nlohmann::json &config_, int steps_) : 
        networks(networks_), processor_config(config_), num_steps(steps_)
    {
        results = nullptr;
        scores = nullptr;
        actual = nullptr;
    }

    ConcurrentQueue<size_t> queue; // queue of network ids to process
    std::vector<Network*>& networks; // reference to a listing of networks to process
    std::vector<std::vector<Spike>> encoded_data; // only encode data once and allow all threads to read
    const nlohmann::json& processor_config; // 
    std::vector<int> *actual; // labels
    int *results; // 2-d array of prediction results
    double *scores; // 1-d array of accuracies
    int num_steps; // number of timesteps for each sample
};

void predict(caspian::Processor &p, Network *net, std::vector<std::vector<Spike>>& spikes, int num_steps, int* ret)
{
    p.load_network(net);

    // Predict each sample by iterating through the encoded data vector (spikes)
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

        // Currently just doing argmax of spike count. In the future, this should accept a decoder object.
        ret[sample] = idx;

        // Clear before next sample
        p.clear_activity();
    }
}

void score(int *predictions, std::vector<int>& y, size_t num, double *score)
{
    size_t correct = 0;

    // sanity check
    if(y.size() != num)
    {
        *score = 0;
        return;
    }

    // count how many correct predictions the network made
    for(size_t i = 0; i < num; i++)
        if(predictions[i] == y[i]) correct++;

    // set the score to be the accuracy percentage
    *score = static_cast<double>(correct) / static_cast<double>(num);
}

void pool_worker(WorkerData *info)
{
    size_t id;
    size_t r_stride = info->encoded_data.size();

    auto processor = caspian::Processor(info->processor_config);

    // keep popping network ids off the queue until everything is processed
    while(info->queue.try_dequeue(id))
    {
        predict(processor, 
                info->networks[id], 
                info->encoded_data, 
                info->num_steps, 
                &(info->results[id * r_stride]));

        if(info->scores != nullptr)
        {
            score(&(info->results[id * r_stride]),
                  *(info->actual),
                  info->encoded_data.size(),
                  &(info->scores[id]));
        }
    }
}

void encode(WorkerData *info, py::array_t<double>& data, EncoderArray *encoder)
{
    // Encode into spikes
    auto d = data.unchecked<2>();

    std::vector<double> dp(d.shape(1));
    for(size_t i = 0; i < d.shape(0); i++)
    {
        // Right now, the data must be copied into a vector piece by piece to pass to the encoder
        for(size_t k = 0; k < d.shape(1); k++)
        {
            dp[k] = d(i, k);
        }

        info->encoded_data.push_back(encoder->get_spikes(dp));
    }
}

void run_pool(WorkerData *info, int num_threads)
{
    // Fill queue
    for(size_t i = 0; i < info->networks.size(); i++)
    {
        info->queue.enqueue(i);
    }

    // Launch worker threads
    std::vector<std::thread> threads;
    for(size_t i = 0; i < num_threads; i++)
    {
        threads.emplace_back(pool_worker, info);
    }

    // Wait for completion
    for(size_t i = 0; i < threads.size(); i++)
    {
        threads[i].join();
    }
}

py::array_t<double> score_all_pool(const nlohmann::json &j, EncoderArray *encoder,
        std::vector<Network*> networks, py::array_t<double>data, std::vector<int> y, int num_steps, int num_threads)
{
    auto info = std::make_unique<WorkerData>(networks, j, num_steps);

    // encode all the data to spikes
    encode(info.get(), data, encoder);

    // Allocate results array
    const int results_size = networks.size() * info->encoded_data.size();
    info->results = new int[results_size];
    info->scores = new double[networks.size()];
    info->actual = &y;

    // The accuracy scores will be returned as a Python buffer to avoid a copy, so 
    // we need to tell Python how to deallocate the buffer when done
    py::capsule free_when_done(info->scores, [](void *f) {
        double *ptr = reinterpret_cast<double *>(f);
        delete[] ptr;
    });

    run_pool(info.get(), num_threads);

    // delete the results; we don't need them
    delete[] info->results;

    // return buffer of the scores (basically like a numpy array)
    return py::array_t<double>(
        {networks.size()}, // shape
        {sizeof(double)}, // strides
        info->scores, // data ptr
        free_when_done); // deallocator object
}

py::array_t<int> predict_all_pool(const nlohmann::json &j, EncoderArray *encoder,
        std::vector<Network*> networks, py::array_t<double>data, int num_steps, int num_threads)
{
    auto info = std::make_unique<WorkerData>(networks, j, num_steps);

    // encode all the data to spikes
    encode(info.get(), data, encoder);

    // Allocate results array
    const int results_size = networks.size() * info->encoded_data.size();
    info->results = new int[results_size];

    py::capsule free_when_done(info->results, [](void *f) {
        int *ptr = reinterpret_cast<int *>(f);
        delete[] ptr;
    });

    run_pool(info.get(), num_threads);

    // return buffer of the predictions (basically like a numpy ndarray)
    return py::array_t<int>(
        {networks.size(), info->encoded_data.size()}, // shape
        {sizeof(int) * info->encoded_data.size(), sizeof(int)}, // strides
        info->results, // data ptr
        free_when_done); // deallocator object
}


void bind_fast_infer(py::module &m)
{
    m.def("fast_predict", &predict_all_pool,
            py::arg("proc_config"), py::arg("encoder"), py::arg("networks"),
            py::arg("data"), py::arg("num_steps"), py::arg("num_threads") = 4);

    m.def("fast_accuracy", &score_all_pool,
            py::arg("proc_config"), py::arg("encoder"), py::arg("networks"),
            py::arg("data"), py::arg("y"), py::arg("num_steps"), py::arg("num_threads") = 4);
}
