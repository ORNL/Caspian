#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "network.hpp"
#include "constants.hpp"

#include "nlohmann/json.hpp"
#include "pybind_json.hpp"

namespace py = pybind11;
namespace csp = caspian;

void bind_network(py::module &m) {
    /* Network Bindings
     *   Neurons, Synapses, Networks, and associatied types/utilities
     */
    py::class_<csp::Neuron>(m, "Neuron")
        .def(py::init<>())
        .def(py::init<int16_t, uint32_t, int8_t, uint8_t>(),
            py::arg("threshold"), py::arg("nid"), py::arg("leak") = -1, py::arg("delay") = 0
        )

        .def("__iter__", [](csp::Neuron &n) {
            return py::make_iterator(n.synapses.begin(), n.synapses.end());
        })

        .def("__len__", [](csp::Neuron &n) { 
            return n.synapses.size();
        })

        .def("__getitem__", [](csp::Neuron &n, uint32_t key) -> csp::Synapse* {
            auto it = n.synapses.find(key);

            if(it == n.synapses.end())
                throw py::index_error();

            return &(it->second);
        }, py::return_value_policy::reference)

        .def("dump", &csp::Neuron::to_json)

        .def_readwrite("leak", &csp::Neuron::leak)
        .def_readwrite("delay", &csp::Neuron::delay)
        .def_readwrite("threshold", &csp::Neuron::threshold)
        .def_readonly("charge", &csp::Neuron::charge)
        .def_readonly("nid", &csp::Neuron::id)
        .def_readonly("input_id", &csp::Neuron::input_id)
        .def_readonly("output_id", &csp::Neuron::output_id);

    py::class_<csp::Synapse>(m, "Synapse")
        .def(py::init<>())
        .def(py::init<int16_t, uint8_t>(), py::arg("weight"), py::arg("delay") = 0)
        .def_readwrite("weight", &csp::Synapse::weight)
        .def_readwrite("delay", &csp::Synapse::delay);

    py::class_<csp::Network>(m, "Network")
        .def(py::init<size_t>(), py::arg("size") = 0)

        /* Serialization */
        .def("from_str", &csp::Network::from_str)
        .def("to_str", &csp::Network::to_str)
        .def("to_gml", &csp::Network::to_gml)

        .def("dump", &csp::Network::to_json)
        .def("load", &csp::Network::from_json)

        /* Support Pythonic methods */
        .def("__repr__", &csp::Network::to_str)

        .def("__iter__", [](csp::Network &net) { 
            return py::make_iterator(net.begin(), net.end()); 
        }, py::keep_alive<0,1>())

        .def("__len__", [](csp::Network &net) { 
            return net.size(); 
        })
        
        .def("__contains__", [](csp::Network &net, csp::Neuron &n) {
            return net.is_neuron(n.id);
        })

        .def("__getitem__", [](csp::Network &net, uint32_t key) { 
            if(!net.is_neuron(key))
                throw py::index_error();

            return net.get_neuron_ptr(key);
        }, py::return_value_policy::reference)

        .def("__setitem__", [](csp::Network &net, uint32_t key, csp::Neuron &n) {
            if(key > net.get_max_size()) 
                throw py::index_error();

            if(!net.is_neuron(n.id)) {
                net.add_neuron(n.id, n.threshold, n.leak);
            } else {
                csp::Neuron &nn = net.get_neuron(n.id);
                nn.threshold = n.threshold;
                nn.leak = n.leak;
            }
        })

        .def("__delitem__", [](csp::Network &net, uint32_t key) {
            net.remove_neuron(key); 
        })
        
        /* Pickle support -- under development */
        .def(py::pickle(
            [](const csp::Network &net){
                std::string str = net.to_str();
                return py::make_tuple(str, 1);
            },
            [](py::tuple t){
                csp::Network net;
                net.from_str(t[0].cast<std::string>());
                return net;
            }
        ))

        /* Neurons */
        .def("add_neuron", 
            (void (csp::Network::*)(uint32_t,int16_t,int8_t,uint8_t)) &csp::Network::add_neuron,
            py::arg("nid"), py::arg("threshold") = 0, py::arg("leak") = -1, py::arg("delay") = 0
        )
        .def("remove_neuron", &csp::Network::remove_neuron, py::arg("nid"))
        .def("is_neuron", &csp::Network::is_neuron, py::arg("nid"))
        .def("get_neuron", &csp::Network::get_neuron_ptr, py::arg("nid"), py::return_value_policy::reference_internal)

        /* Synapses */
        .def("add_synapse", 
            (void (csp::Network::*)(uint32_t,uint32_t,int16_t,uint8_t)) &csp::Network::add_synapse,
            py::arg("from"), py::arg("to"), py::arg("weight"), py::arg("delay") = 0
        )
        .def("remove_synapse", &csp::Network::remove_synapse, py::arg("from"), py::arg("to"))
        .def("is_synapse", &csp::Network::is_synapse, py::arg("from"), py::arg("to"))
        .def("get_synapse", &csp::Network::get_synapse_ptr, py::arg("from"), py::arg("to"), py::return_value_policy::reference_internal)

        /* I/O */ 
        .def("set_input", &csp::Network::set_input, py::arg("nid"), py::arg("input_id"))
        .def("set_output", &csp::Network::set_output, py::arg("nid"), py::arg("output_id"))
        .def("get_input", &csp::Network::get_input, py::arg("input_id"))
        .def("get_output", &csp::Network::get_output, py::arg("output_id"))
        .def("num_inputs", &csp::Network::num_inputs)
        .def("num_outputs", &csp::Network::num_outputs)

        /* Random methods -- literally */
        .def("make_random", &csp::Network::make_random, 
                py::arg("n_inputs"), py::arg("n_outputs"), py::arg("seed"),
                py::arg("n_input_synapses") = -1, 
                py::arg("n_output_synapses") = -1,
                py::arg("n_hidden_synapses") = -1,
                py::arg("n_hidden_synapses_max") = -1,
                py::arg("inhibitory_percentage") = 0.2,
                py::arg("threshold_range") = std::make_pair(csp::constants::MIN_THRESHOLD, csp::constants::MAX_THRESHOLD),
                py::arg("leak_range") = std::make_pair(csp::constants::MIN_LEAK, csp::constants::MAX_LEAK),
                py::arg("weight_range") = std::make_pair(0, csp::constants::MAX_WEIGHT),
                py::arg("delay_range") = std::make_pair(csp::constants::MIN_DELAY, csp::constants::MAX_DELAY))
        .def("get_random_input", &csp::Network::get_random_input)
        .def("get_random_output", &csp::Network::get_random_output)
        .def("get_random_neuron", &csp::Network::get_random_neuron, py::arg("only_hidden") = false)
        .def("get_random_synapse", &csp::Network::get_random_synapse)
        .def("get_neuron_list", &csp::Network::get_neuron_list)
        .def("get_synapse_list", &csp::Network::get_synapse_list)
        .def("num_neurons", &csp::Network::num_neurons)
        .def("num_synapses", &csp::Network::num_synapses)

        /* misc */
        .def("get_metric", &csp::Network::get_metric, py::arg("metric"))
        .def("reset", &csp::Network::reset)
        .def("clear_activity", &csp::Network::clear_activity)
        .def("prune", &csp::Network::prune, py::arg("io_prune") = false)
        .def("get_time", &csp::Network::get_time)
        .def_readwrite("soft_reset", &csp::Network::soft_reset);
}
