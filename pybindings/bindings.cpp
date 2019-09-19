#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "tennlab.hpp"

#include "network.hpp"
#include "backend.hpp"
#include "simulator.hpp"
#include "spike_encoder.hpp"

#include "pybind_json.hpp"

namespace py = pybind11;
namespace csp = caspian;

void bind_network(py::module &m);
void bind_backend(py::module &m);
void bind_tennlab_processor(py::module &m);

PYBIND11_MODULE(caspian, m) {
    m.doc() = "CASPIAN for Python";

    /* Import TENNLab Framework so that we can access its types */
    py::module::import("tennlab");

    /* Neuron, Synapse, Network */
    bind_network(m);

    /* Backend, Simulator */
    bind_backend(m);

    /* TENNLab Processor */
    bind_tennlab_processor(m);
    
    /* SpikeEncoder utility class */
    py::enum_<csp::SpikeVariable>(m, "SpikeVariable", py::arithmetic())
        .value("NumSpikes", csp::SpikeVariable::NumSpikes)
        .value("Interval", csp::SpikeVariable::Interval);

    py::class_<csp::SpikeEncoder>(m, "SpikeEncoder")
        .def(py::init<int,int,double,double,csp::SpikeVariable>(), 
                py::arg("n_spikes") = 10, 
                py::arg("interval") = 1,
                py::arg("dmin") = 0.0f,
                py::arg("dmax") = 1.0f,
                py::arg("sv") = csp::SpikeVariable::NumSpikes)
        .def(py::pickle(
            [](const csp::SpikeEncoder &en) {
                return py::make_tuple(en.n_spikes, en.interval, en.dmin, en.dmax, en.sv);
            },
            [](py::tuple t) {
                int n_spikes = t[0].cast<int>();
                int interval = t[1].cast<int>();
                double dmin  = t[2].cast<double>();
                double dmax  = t[3].cast<double>();
                csp::SpikeVariable sv = t[4].cast<csp::SpikeVariable>();

                return csp::SpikeEncoder(n_spikes, interval, dmin, dmax, sv);
            }
        ))
        .def("encode", &csp::SpikeEncoder::encode, py::arg("data"));

}
