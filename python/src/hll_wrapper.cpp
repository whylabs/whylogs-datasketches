/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "hll.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

namespace py = pybind11;

namespace datasketches {
namespace python {

hll_sketch hll_sketch_deserialize(py::bytes skBytes) {
  std::string skStr = skBytes; // implicit cast  
  return hll_sketch::deserialize(skStr.c_str(), skStr.length());
}

py::object hll_sketch_serialize_compact(const hll_sketch& sk) {
  auto serResult = sk.serialize_compact();
  return py::bytes((char*)serResult.data(), serResult.size());
}

py::object hll_sketch_serialize_updatable(const hll_sketch& sk) {
  auto serResult = sk.serialize_updatable();
  return py::bytes((char*)serResult.data(), serResult.size());
}

template<typename T>
void hll_sketch_update_ndarray(hll_sketch& sk, py::array_t<T, py::array::c_style | py::array::forcecast> items) {
  if (items.ndim() != 1) {
    throw std::invalid_argument("input data must have only one dimension. Found: "
          + std::to_string(items.ndim()));
  }

  auto data = items.template unchecked<1>();
  for (uint32_t i = 0; i < data.size(); ++i) {
    sk.update(data(i));
  }
}

void hll_sketch_update_str_list(hll_sketch& sk, const py::list items) {
  for(py::handle obj : items) {
    sk.update(obj.cast<std::string>());
  }
}

void hll_sketch__update_int_list(hll_sketch& sk, const py::list items) {
  for(py::handle obj : items) {
    sk.update(std::to_string(obj.cast<int64_t>()));
  }
}

void hll_sketch_update_double_list(hll_sketch& sk, const py::list items) {
  for(py::handle obj : items) {
    sk.update(std::to_string(obj.cast<double>()));
  }
}
}
}

namespace dspy = datasketches::python;

void init_hll(py::module &m) {
  using namespace datasketches;

  py::enum_<target_hll_type>(m, "tgt_hll_type", "Target HLL flavor")
    .value("HLL_4", HLL_4)
    .value("HLL_6", HLL_6)
    .value("HLL_8", HLL_8)
    .export_values();

  py::class_<hll_sketch>(m, "hll_sketch")
    .def(py::init<uint8_t>(), py::arg("lg_k"))
    .def(py::init<uint8_t, target_hll_type>(), py::arg("lg_k"), py::arg("tgt_type"))
    .def(py::init<uint8_t, target_hll_type, bool>(), py::arg("lg_k"), py::arg("tgt_type"), py::arg("start_max_size")=false)
    .def_static("deserialize", &dspy::hll_sketch_deserialize,
         "Reads a bytes object and returns the corresponding hll_sketch")
    .def("serialize_compact", &dspy::hll_sketch_serialize_compact,
         "Serializes the sketch into a bytes object, compressiong the exception table if HLL_4")
    .def("serialize_updatable", &dspy::hll_sketch_serialize_updatable,
         "Serializes the sketch into a bytes object")
    .def("__str__", (std::string (hll_sketch::*)(bool,bool,bool,bool) const) &hll_sketch::to_string,
         py::arg("summary")=true, py::arg("detail")=false, py::arg("aux_detail")=false, py::arg("all")=false,
         "Produces a string summary of the sketch")
    .def("to_string", (std::string (hll_sketch::*)(bool,bool,bool,bool) const) &hll_sketch::to_string,
         py::arg("summary")=true, py::arg("detail")=false, py::arg("aux_detail")=false, py::arg("all")=false,
         "Produces a string summary of the sketch")
    .def_property_readonly("lg_config_k", &hll_sketch::get_lg_config_k, "Configured lg_k value for the sketch")
    .def_property_readonly("tgt_type", &hll_sketch::get_target_type, "Returns the HLL type (4, 6, or 8) when in estimation mode")
    .def("get_estimate", &hll_sketch::get_estimate,
         "Estimate of the distinct count of the input stream")
    .def("get_lower_bound", &hll_sketch::get_lower_bound, py::arg("num_std_devs"),
         "Returns the approximate lower error bound given the specified number of standard deviations in {1, 2, 3}")
    .def("get_upper_bound", &hll_sketch::get_upper_bound, py::arg("num_std_devs"),
         "Returns the approximate upper error bound given the specified number of standard deviations in {1, 2, 3}")
    .def("is_compact", &hll_sketch::is_compact,
         "True if the sketch is compact, otherwise False")
    .def("is_empty", &hll_sketch::is_empty,
         "True if the sketch is empty, otherwise False")
    .def("get_updatable_serialization_bytes", &hll_sketch::get_updatable_serialization_bytes,
         "Returns the size of the serialized sketch")
    .def("get_compact_serialization_bytes", &hll_sketch::get_compact_serialization_bytes,
         "Returns the size of the serialized sketch when compressing the exception table if HLL_4")
    .def("reset", &hll_sketch::reset,
         "Resets the sketch to the empty state in coupon colleciton mode")
    .def("update", (void (hll_sketch::*)(int64_t)) &hll_sketch::update, py::arg("datum"),
         "Updates the sketch with the given integral value")
    .def("update", (void (hll_sketch::*)(double)) &hll_sketch::update, py::arg("datum"),
         "Updates the sketch with the given floating point value")
    .def("update", (void (hll_sketch::*)(const std::string&)) &hll_sketch::update, py::arg("datum"),
         "Updates the sketch with the given string value")
    .def_static("get_max_updatable_serialization_bytes", &hll_sketch::get_max_updatable_serialization_bytes,
         py::arg("lg_k"), py::arg("tgt_type"),
         "Provides a likely upper bound on serialization size for the given paramters")
    .def_static("get_rel_err", &hll_sketch::get_rel_err,
         py::arg("upper_bound"), py::arg("unioned"), py::arg("lg_k"), py::arg("num_std_devs"),
         "Retuns the a priori relative error bound for the given parameters")
    .def("update_np", &dspy::hll_sketch_update_ndarray<double>, py::arg("array"),
         "Update with a np array of doubles")
    .def("update_np", &dspy::hll_sketch_update_ndarray<int64_t>, py::arg("array"),
         "Update with a np array of ints")
    .def("update_str_list", &dspy::hll_sketch_update_str_list, py::arg("str_list"),
         "Update list of strings in batch")
    .def("update_int_list", &dspy::hll_sketch__update_int_list, py::arg("int_list"),
         "Update list of strings in ints")
    .def("update_double_list", &dspy::hll_sketch_update_double_list, py::arg("double_list"),
         "Update list of strings in double")
    ;

  py::class_<hll_union>(m, "hll_union")
    .def(py::init<uint8_t>(), py::arg("lg_max_k"))
    .def_property_readonly("lg_config_k", &hll_union::get_lg_config_k, "Configured lg_k value for the union")
    .def_property_readonly("tgt_type", &hll_union::get_target_type, "Returns the HLL type (4, 6, or 8) when in estimation mode")
    .def("get_estimate", &hll_union::get_estimate,
         "Estimate of the distinct count of the input stream")
    .def("get_lower_bound", &hll_union::get_lower_bound, py::arg("num_std_devs"),
         "Returns the approximate lower error bound given the specified number of standard deviations in {1, 2, 3}")
    .def("get_upper_bound", &hll_union::get_upper_bound, py::arg("num_std_devs"),
         "Returns the approximate upper error bound given the specified number of standard deviations in {1, 2, 3}")
    .def("is_empty", &hll_union::is_empty,
         "True if the union is empty, otherwise False")    
    .def("reset", &hll_union::reset,
         "Resets the union to the empty state")
    .def("get_result", &hll_union::get_result, py::arg("tgt_type")=HLL_4,
         "Returns a sketch of the target type representing the current union state")
    .def<void (hll_union::*)(const hll_sketch&)>("update", &hll_union::update, py::arg("sketch"),
         "Updates the union with the given HLL sketch")
    .def<void (hll_union::*)(int64_t)>("update", &hll_union::update, py::arg("datum"),
         "Updates the union with the given integral value")
    .def<void (hll_union::*)(double)>("update", &hll_union::update, py::arg("datum"),
         "Updates the union with the given floating point value")
    .def<void (hll_union::*)(const std::string&)>("update", &hll_union::update, py::arg("datum"),
         "Updates the union with the given string value")
    .def_static("get_rel_err", &hll_union::get_rel_err,
         py::arg("upper_bound"), py::arg("unioned"), py::arg("lg_k"), py::arg("num_std_devs"),
         "Retuns the a priori relative error bound for the given parameters")
    ;
}
