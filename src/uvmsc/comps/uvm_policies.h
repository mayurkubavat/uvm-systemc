//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2015-2024 NVIDIA Corporation
//   Copyright 2026 Mayur Kubavat
//   All Rights Reserved Worldwide
//
//   Licensed under the Apache License, Version 2.0 (the
//   "License"); you may not use this file except in
//   compliance with the License.  You may obtain a copy of
//   the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in
//   writing, software distributed under the License is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//   CONDITIONS OF ANY KIND, either express or implied.  See
//   the License for the specific language governing
//   permissions and limitations under the License.
//----------------------------------------------------------------------

#ifndef UVM_POLICIES_H_
#define UVM_POLICIES_H_

#include <sstream>
#include <string>

namespace uvm {

//----------------------------------------------------------------------
// Policy classes
//
// Policy classes are used to implement polymorphic operations that
// differ between built-in types and class-based types. The comparator
// classes use them as type parameters to compare and print
// transactions.
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// CLASS: uvm_built_in_comp
//
//! This policy class is used to compare built-in types, and any other
//! type for which operator== is defined.
//----------------------------------------------------------------------

template <typename T = int>
class uvm_built_in_comp
{
 public:
  static bool comp( const T& a, const T& b )
  {
    return a == b;
  }
};

//----------------------------------------------------------------------
// CLASS: uvm_built_in_converter
//
//! This policy class is used to convert built-in types to strings, and
//! any other type for which operator<< to an output stream is defined.
//----------------------------------------------------------------------

template <typename T = int>
class uvm_built_in_converter
{
 public:
  static std::string convert2string( const T& t )
  {
    std::ostringstream str;
    str << t;
    return str.str();
  }
};

//----------------------------------------------------------------------
// CLASS: uvm_class_comp
//
//! This policy class is used to compare two objects of the same type.
//! It calls the member function compare of the type, which objects
//! derived from uvm_object provide.
//----------------------------------------------------------------------

template <typename T>
class uvm_class_comp
{
 public:
  static bool comp( const T& a, const T& b )
  {
    return a.compare(b);
  }
};

//----------------------------------------------------------------------
// CLASS: uvm_class_converter
//
//! This policy class is used to convert a class object to a string.
//! It calls the member function convert2string of the type, which
//! objects derived from uvm_object provide.
//----------------------------------------------------------------------

template <typename T>
class uvm_class_converter
{
 public:
  static std::string convert2string( const T& t )
  {
    return t.convert2string();
  }
};

} // namespace uvm

#endif // UVM_POLICIES_H_
