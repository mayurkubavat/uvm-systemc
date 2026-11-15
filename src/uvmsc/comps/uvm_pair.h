//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2015-2024 NVIDIA Corporation
//   Copyright 2014 Semifore
//   Copyright 2011 Synopsys, Inc.
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

#ifndef UVM_PAIR_H_
#define UVM_PAIR_H_

#include <sstream>
#include <string>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/macros/uvm_message_defines.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_class_pair
//
//! Container holding two objects whose types are specified by the type
//! parameters T1 and T2. Both types are expected to be derived from
//! uvm_object.
//----------------------------------------------------------------------

template <typename T1, typename T2 = T1>
class uvm_class_pair : public uvm_object
{
 public:

  //! The first object of the pair
  T1 first;

  //! The second object of the pair
  T2 second;

  explicit uvm_class_pair( const std::string& name = "" )
  : uvm_object(name)
  {}

  uvm_class_pair( const std::string& name, const T1& f, const T2& s )
  : uvm_object(name), first(f), second(s)
  {}

  virtual const std::string get_type_name() const
  {
    return "uvm_class_pair #(T1,T2)";
  }

  virtual uvm_object* create( const std::string& name = "" )
  {
    return new uvm_class_pair<T1,T2>(name);
  }

  virtual std::string convert2string() const
  {
    return "pair : " + first.convert2string() + ", " + second.convert2string();
  }

  virtual bool do_compare( const uvm_object& rhs,
                           const uvm_comparer* comparer = nullptr ) const
  {
    const uvm_class_pair<T1,T2>* rhs_ =
      dynamic_cast<const uvm_class_pair<T1,T2>*>(&rhs);

    if (rhs_ == nullptr)
    {
      UVM_ERROR("WRONG_TYPE", "do_compare: rhs argument is not of type '" +
        get_type_name() + "'");
      return false;
    }

    return first.compare(rhs_->first) && second.compare(rhs_->second);
  }

  virtual void do_copy( const uvm_object& rhs )
  {
    const uvm_class_pair<T1,T2>* rhs_ =
      dynamic_cast<const uvm_class_pair<T1,T2>*>(&rhs);

    if (rhs_ == nullptr)
    {
      UVM_FATAL("WRONG_TYPE", "do_copy: rhs argument is not of type '" +
        get_type_name() + "'");
      return;
    }

    first.copy(rhs_->first);
    second.copy(rhs_->second);
  }

}; // class uvm_class_pair


//----------------------------------------------------------------------
// CLASS: uvm_built_in_pair
//
//! Container holding two values of built-in types, or of any other
//! types for which operator== and operator<< to an output stream are
//! defined. The types are specified by the type parameters T1 and T2.
//----------------------------------------------------------------------

template <typename T1 = int, typename T2 = T1>
class uvm_built_in_pair : public uvm_object
{
 public:

  //! The first value of the pair
  T1 first;

  //! The second value of the pair
  T2 second;

  explicit uvm_built_in_pair( const std::string& name = "" )
  : uvm_object(name), first(), second()
  {}

  virtual const std::string get_type_name() const
  {
    return "uvm_built_in_pair #(T1,T2)";
  }

  virtual uvm_object* create( const std::string& name = "" )
  {
    return new uvm_built_in_pair<T1,T2>(name);
  }

  virtual std::string convert2string() const
  {
    std::ostringstream str;
    str << "built-in pair : " << first << ", " << second;
    return str.str();
  }

  virtual bool do_compare( const uvm_object& rhs,
                           const uvm_comparer* comparer = nullptr ) const
  {
    const uvm_built_in_pair<T1,T2>* rhs_ =
      dynamic_cast<const uvm_built_in_pair<T1,T2>*>(&rhs);

    if (rhs_ == nullptr)
    {
      UVM_ERROR("WRONG_TYPE", "do_compare: rhs argument is not of type '" +
        get_type_name() + "'");
      return false;
    }

    return first == rhs_->first && second == rhs_->second;
  }

  virtual void do_copy( const uvm_object& rhs )
  {
    const uvm_built_in_pair<T1,T2>* rhs_ =
      dynamic_cast<const uvm_built_in_pair<T1,T2>*>(&rhs);

    if (rhs_ == nullptr)
    {
      UVM_FATAL("WRONG_TYPE", "do_copy: rhs argument is not of type '" +
        get_type_name() + "'");
      return;
    }

    first = rhs_->first;
    second = rhs_->second;
  }

}; // class uvm_built_in_pair

} // namespace uvm

#endif // UVM_PAIR_H_
