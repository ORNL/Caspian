#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "framework.hpp"
#include "nlohmann/json.hpp"
#include "pybind_json.hpp"
#include "processor.hpp"

void bind_processor(pybind11::module &m) {
    namespace py  = pybind11;
    namespace csp = caspian;

    using namespace neuro;

    py::module::import("neuro");
    py::object n_processor = (py::object) py::module::import("neuro").attr("Processor");

    py::class_<csp::Processor>(m, "Processor", n_processor)
        .def(py::init<nlohmann::json&>())
        .def("get_backend", &csp::Processor::get_backend, py::return_value_policy::reference_internal)
        .def("get_internal_network", &csp::Processor::get_internal_network, py::return_value_policy::reference_internal)
        .def("get_configuration", &csp::Processor::get_configuration);
}
