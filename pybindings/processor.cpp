#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "framework.hpp"
#include "nlohmann/json.hpp"
#include "pybind_json.hpp"
#include "processor.hpp"

void bind_tennlab_processor(pybind11::module &m) {
    namespace py  = pybind11;
    namespace csp = caspian;

    using namespace TENNLab;

    py::module::import("neuro");
    py::object n_processor = (py::object) py::module::import("neuro").attr("Processor");

    py::class_<csp::Processor>(m, "Processor", n_processor)
        .def(py::init<nlohmann::json&>())
        .def("get_backend", &csp::Processor::Get_Backend, py::return_value_policy::reference_internal)
        .def("get_internal_network", &csp::Processor::Get_Internal_Network, py::return_value_policy::reference_internal)
        .def("get_configuration", &csp::Processor::Get_Configuration);
}
