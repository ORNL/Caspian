#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "framework.hpp"
#include "processor.hpp"

#include <vector>
#include <thread>

using namespace neuro;
namespace py = pybind11;

void predict(const nlohmann::json &j, EncoderArray *encoder, Network* net, 
        py::array_t<double> data, int num_steps, std::vector<int>& ret)
{
    auto p = caspian::Processor(j);
    auto d = data.unchecked<2>();

    for(size_t samples = 0; samples < d.shape(0); samples++)
    {
        std::vector<double> dp;
        for(size_t j = 0; j < d.shape(1); j++)
        {
            dp.push_back(d(samples, j));
        }

        p.load_network(net);

        // Encode
        auto spikes = encoder->get_spikes(dp);
        p.apply_spikes(spikes);

        // Run
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


std::vector<std::vector<int>> predict_all(const nlohmann::json &j, EncoderArray *encoder, 
        std::vector<Network*> networks, py::array_t<double> data, int num_steps)
{
    std::vector<std::vector<int>> ret;
    std::vector<std::thread> threads;

    ret.resize(networks.size());

    for(size_t i = 0; i < networks.size(); i++)
    {
        threads.push_back(std::thread(predict, std::ref(j), encoder, networks[i], data, num_steps, std::ref(ret[i])));
    }

    for(auto &th : threads) th.join();

    /*
    for(Network *net : networks)
    {
        auto r = predict(j, encoder, net, data, num_steps);
        ret.push_back(r);
    }
    */
    
    return ret;
}

void bind_fast_infer(py::module &m)
{
    m.def("fast_predict_all", &predict_all,
            py::arg("proc_config"), py::arg("encoder"), py::arg("networks"), 
            py::arg("data"), py::arg("num_steps"));
}
