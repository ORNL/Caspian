#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <stdexcept>
#include "backend.hpp"
#include "simulator.hpp"
#include "ucaspian.hpp"

namespace py = pybind11;
namespace csp = caspian;

void bind_backend(py::module &m) {
    /* Simulator/Device Bindings
     *   Device, SimDevice, etc.
     */
    py::class_<csp::Backend>(m, "Backend")
        .def("apply_input", 
            (void (csp::Backend::*)(int,int16_t,uint64_t)) &csp::Backend::apply_input,
            py::arg("input_id"), py::arg("charge"), py::arg("t")
        )

        .def("set_debug", &csp::Backend::set_debug)

        /* This uses the native python object rather than converting because
         * conversion from py::list -> std::vector is a copying operation */
        .def("apply_inputs", [](csp::Backend &dev, py::list l) {
            int input_id = 0;

            for(auto ob : l) {
                try {
                    /* try to cast item in list to another py::list
                     * failure will generate an exception 
                     * which eventually becomes a ValueError in Python */
                    py::list ll = ob.cast<py::list>();

                    if(ll.size() > 0)
                    {
                        for(auto val : ll) {
                            py::tuple tv = val.cast<py::tuple>();
                            int w = tv[0].cast<int>();
                            int t = tv[1].cast<int>();

                            dev.apply_input(input_id, w, t);
                        }
                    }

                    input_id++;

                } catch(...) {
                    /* This exception will be presented as a ValueError in Python */
                    throw std::invalid_argument("apply_inputs error in casting list of list of tuples");
                }
            }
        })

        .def("apply_dvs_events", 
        [](csp::Backend &dev,
           const std::vector<int> x, 
           const std::vector<int> y, 
           const std::vector<int> p, 
           const std::vector<double> t, 
           std::pair<int,int> dims,
           bool use_polarity) {

            // error check
            if(x.size() != y.size() || y.size() != t.size() || (use_polarity && t.size() != p.size())) {
                throw std::runtime_error("[apply_dvs_events] x, y, p, and t must have matching length");
            }

            // Value: caspian::constants::MAX_DEVICE_INPUT
            
            uint32_t max_x, max_y;
            std::tie(max_x, max_y) = dims;
            uint32_t frame_size = max_x * max_y;

            // Generate and immediately apply spikes
            for(size_t i = 0; i < x.size(); ++i)
            {
                auto nid = y[i] * max_x + x[i];
                if(use_polarity) nid += p[i] * frame_size;
                dev.apply_input(nid, caspian::constants::MAX_DEVICE_INPUT, uint64_t(floor(t[i])));
            }

        }, py::arg("x"), py::arg("y"), py::arg("p"), py::arg("t"), py::arg("dims"), py::arg("use_polarity") = true)

        .def("collect_all_spikes", &csp::Backend::collect_all_spikes, py::arg("collect") = true)
        .def("get_all_spikes", &csp::Backend::get_all_spikes)

        .def("configure", &csp::Backend::configure)
        .def("configure_multi", &csp::Backend::configure_multi)
        .def("simulate", &csp::Backend::simulate)
        .def("update", &csp::Backend::update)
        .def("get_metric", &csp::Backend::get_metric)
        .def("get_time", &csp::Backend::get_time)
        .def("reset", &csp::Backend::reset)
        .def("clear_activity", &csp::Backend::clear_activity)
        .def("track_aftertime", &csp::Backend::track_aftertime, py::arg("output_id"), py::arg("aftertime"))
        .def("track_timing", &csp::Backend::track_timing, py::arg("output_id"), py::arg("do_tracking") = true)
        .def("get_output_count", &csp::Backend::get_output_count, py::arg("output_id"), py::arg("network_id") = 0)
        .def("get_last_output_time", &csp::Backend::get_last_output_time, py::arg("output_id"), py::arg("network_id") = 0)
        .def("get_all_output_counts", [](csp::Backend &dev, int n_outputs, int network_id) {
            std::vector<int> outputs(n_outputs);
            for(int i = 0; i < n_outputs; ++i) {
                outputs[i] = dev.get_output_count(i, network_id);
            }
            return outputs;
        }, py::arg("n_outputs"), py::arg("network_id") = 0)
        .def("get_output_max_count", [](csp::Backend &dev, int n_outputs, int network_id) {
            int max_idx = 0;
            int max_val = 0;

            for(int i = 0; i < n_outputs; ++i) {
                int cnt = dev.get_output_count(i, network_id);
                if(cnt > max_val) {
                    max_idx = i;
                    max_val = cnt;
                }
            }

            return std::make_tuple(max_idx, max_val);
        }, py::arg("n_outputs"), py::arg("network_id") = 0)
        .def("get_outputs", &csp::Backend::get_output_values, py::arg("output_id"), py::arg("network_id") = 0);

    py::class_<csp::Simulator, csp::Backend>(m, "Simulator")
        .def(py::init<bool>(), py::arg("debug") = false)
        
        .def("spike_data", [](csp::Simulator &sim) {
            std::vector<int> times, ids;
            int i = 0;

            while(sim.get_output_count(i) >= 0) {
                auto outputs = sim.get_output_values(i);
                for(auto out : outputs) {
                    times.push_back(out);
                    ids.push_back(i);
                }
                i++;
            }

            return std::make_pair(times, ids);
        });

#ifdef WITH_USB
    py::class_<csp::UsbCaspian, csp::Backend>(m, "UsbCaspian")
        .def(py::init<bool>(), py::arg("debug") = false)

        .def("clear_config", [](csp::UsbCaspian &sim) {
            return sim.configure(nullptr);
        })


        .def("spike_data", [](csp::UsbCaspian &sim) {
            std::vector<int> times, ids;
            int i = 0;

            while(sim.get_output_count(i) >= 0) {
                auto outputs = sim.get_output_values(i);
                for(auto out : outputs) {
                    times.push_back(out);
                    ids.push_back(i);
                }
                i++;
            }

            return std::make_pair(times, ids);
        });
#endif
        
}
