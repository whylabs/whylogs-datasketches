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

#include <sstream>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include "cpc_sketch.hpp"
#include "cpc_union.hpp"
#include "cpc_common.hpp"
#include "common_defs.hpp"

namespace py = pybind11;

namespace datasketches {
namespace python {

cpc_sketch* cpc_sketch_deserialize(py::bytes skBytes) {
  std::string skStr = skBytes; // implicit cast
  return new cpc_sketch(cpc_sketch::deserialize(skStr.c_str(), skStr.length()));
}

py::object cpc_sketch_serialize(const cpc_sketch& sk) {
  auto serResult = sk.serialize();
  return py::bytes((char*)serResult.data(), serResult.size());
}

cpc_sketch* cpc_union_get_result(const cpc_union& u) {
  return new cpc_sketch(u.get_result());
}

template<typename T>
void cpc_sketch_update(cpc_sketch& sk, py::array_t<T, py::array::c_style | py::array::forcecast> items) {
  if (items.ndim() != 1) {
    throw std::invalid_argument("input data must have only one dimension. Found: "
          + std::to_string(items.ndim()));
  }
  
  auto data = items.template unchecked<1>();
  for (uint32_t i = 0; i < data.size(); ++i) {
    sk.update(data(i));
  }
}

void cpc_sketch_update_str_list(cpc_sketch& sk, const py::list items) {
  for(py::handle obj : items) {
    sk.update(obj.cast<std::string>());
  }
}

template<typename T>
void cpc_sketch_update_list(cpc_sketch& sk, const py::list items) {
  for(py::handle obj : items) {
    sk.update(obj.cast<T>());    
  }
}

}
}

namespace dspy = datasketches::python;

void init_cpc(py::module &m) {
  using namespace datasketches;

  py::class_<cpc_sketch>(m, "cpc_sketch")
    .def(py::init<uint8_t, uint64_t>(), py::arg("lg_k")=cpc_constants::DEFAULT_LG_K, py::arg("seed")=DEFAULT_SEED)
    .def(py::init<const cpc_sketch&>())
    .def("__str__", &cpc_sketch::to_string,
         "Produces a string summary of the sketch")
    .def("to_string", &cpc_sketch::to_string,
         "Produces a string summary of the sketch")
    .def("serialize", &dspy::cpc_sketch_serialize,
         "Serializes the sketch into a bytes object")
    .def_static("deserialize", &dspy::cpc_sketch_deserialize,
         "Reads a bytes object and returns the corresponding cpc_sketch")
    .def<void (cpc_sketch::*)(uint64_t)>("update", &cpc_sketch::update, py::arg("datum"),
         "Updates the sketch with the given 64-bit integer value")
    .def<void (cpc_sketch::*)(double)>("update", &cpc_sketch::update, py::arg("datum"),
         "Updates the sketch with the given 64-bit floating point")
    .def<void (cpc_sketch::*)(const std::string&)>("update", &cpc_sketch::update, py::arg("datum"),
         "Updates the sketch with the given string")
    .def("is_empty", &cpc_sketch::is_empty,
         "Returns True if the sketch is empty, otherwise Dalse")
    .def("get_estimate", &cpc_sketch::get_estimate,
         "Estimate of the distinct count of the input stream")
    .def("get_lower_bound", &cpc_sketch::get_lower_bound, py::arg("kappa"),
         "Returns an approximate lower bound on the estimate for kappa values in {1, 2, 3}, roughly corresponding to standard deviations")
    .def("get_upper_bound", &cpc_sketch::get_upper_bound, py::arg("kappa"),
         "Returns an approximate upper bound on the estimate for kappa values in {1, 2, 3}, roughly corresponding to standard deviations")
    .def("update_np", &dspy::cpc_sketch_update<double>, py::arg("array"),
         "Update with a np array of doubles")
    .def("update_np", &dspy::cpc_sketch_update<int64_t>, py::arg("array"),
         "Update with a np array of ints")
    .def("update_str_list", &dspy::cpc_sketch_update_list<std::string>, py::arg("str_list"),
         "Update list of strings")
    .def("update_int_list", &dspy::cpc_sketch_update_list<int64_t>, py::arg("int_list"),
         "Update list of ints")
    .def("update_double_list", &dspy::cpc_sketch_update_list<double>, py::arg("double_list"),
         "Update list of double")

    ;

  py::class_<cpc_union>(m, "cpc_union")
    .def(py::init<uint8_t, uint64_t>(), py::arg("lg_k"), py::arg("seed")=DEFAULT_SEED)
    .def(py::init<const cpc_union&>())
    .def("update", (void (cpc_union::*)(const cpc_sketch&)) &cpc_union::update, py::arg("sketch"),
         "Updates the union with the provided CPC sketch")
    .def("get_result", &dspy::cpc_union_get_result,
         "Returns a CPC sketch with the result of the union")
    ;
}
