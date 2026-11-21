//----------------------------------------------------------------------
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2010-2018 Synopsys, Inc.
//   Copyright 2013-2024 NVIDIA Corporation
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

#ifndef UVM_ENUM_WRAPPER_H_
#define UVM_ENUM_WRAPPER_H_

#include <cstddef>
#include <initializer_list>
#include <map>
#include <string>

#include "uvmsc/base/uvm_object_globals.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_enum_wrapper<T>
//
//! The class uvm_enum_wrapper<T> is a utility mechanism provided as a
//! convenience to the end user. It provides a member function from_name,
//! which is the logical inverse of the name of an enumerator.
//!
//! In SystemVerilog, every enumeration type knows the names of its
//! values. C++ enumerations do not, so the names of the values of an
//! enumeration type T shall be registered before from_name can find
//! them. UVM-SystemC offers two ways to register names:
//!
//! - The macro UVM_ENUM_REGISTER(T, ...) registers the enumerators given
//!   as the remaining arguments, under their own names. Qualifiers, such
//!   as the name of a scoped enumeration, are not part of the name.
//!   The macro can be used at namespace scope or in a function body.
//!
//! - The static member function add registers a single name for a value.
//!
//! For example:
//!
//!   enum class color_e { RED, GREEN, BLUE };
//!   UVM_ENUM_REGISTER(color_e, color_e::RED, color_e::GREEN, color_e::BLUE)
//!
//!   color_e c;
//!   if (uvm::uvm_enum_wrapper<color_e>::from_name("GREEN", c)) ...
//!
//! The names of the enumeration types uvm_active_passive_enum,
//! uvm_verbosity, uvm_severity, uvm_sequencer_arb_mode, uvm_phase_state
//! and uvm_radix_enum are registered by UVM-SystemC.
//----------------------------------------------------------------------

template <typename T = uvm_active_passive_enum>
class uvm_enum_wrapper
{
 public:
  //! Attempts to convert the string name into an enumeration value of
  //! type T. If the conversion is successful, the member function shall
  //! return true and assign the value to the argument value. Otherwise,
  //! it shall return false and value is unchanged.
  static bool from_name( const std::string& name, T& value )
  {
    m_init();

    typename std::map<std::string, T>::const_iterator it = m_map().find(name);
    if (it == m_map().end())
      return false;

    value = it->second;
    return true;
  }

  //! UVM-SystemC addition: registers name as the name of value. A value
  //! may have more than one name. Returns false if the name was
  //! registered already for another value, in which case the name is
  //! not changed; returns true otherwise.
  static bool add( const std::string& name, T value )
  {
    m_init();

    typename std::map<std::string, T>::iterator it = m_map().find(name);
    if (it != m_map().end())
      return it->second == value;

    m_map()[name] = value;
    return true;
  }

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  // Registers the values with the names in the comma separated list
  // names, as created by the macro UVM_ENUM_REGISTER. Returns false if
  // the number of names and values differ or a name is registered for
  // another value.
  static bool m_add_names( const char* names, std::initializer_list<T> values )
  {
    bool ok = true;
    const std::string s(names);
    typename std::initializer_list<T>::const_iterator v = values.begin();
    std::size_t start = 0;
    int depth = 0;

    for (std::size_t i = 0; i <= s.length(); i++)
    {
      if (i < s.length())
      {
        const char c = s[i];
        if (c == '(' || c == '[' || c == '{' || c == '<') depth++;
        if (c == ')' || c == ']' || c == '}' || c == '>') depth--;
        if (c != ',' || depth > 0)
          continue;
      }

      if (v == values.end())
        return false;

      ok = add(m_strip(s.substr(start, i - start)), *v) && ok;
      ++v;
      start = i + 1;
    }

    return ok && v == values.end();
  }

 protected:
  // prevents accidental instantiations
  uvm_enum_wrapper() {}

  // Registers the names of the UVM enumeration types. Specialized for
  // these types; does nothing for any other type.
  static void m_init_map() {}

 private:
  static void m_init()
  {
    static bool init_done = false;
    if (!init_done)
    {
      init_done = true;
      m_init_map();
    }
  }

  static std::map<std::string, T>& m_map()
  {
    static std::map<std::string, T> map;
    return map;
  }

  // removes white space and qualifiers from an enumerator name
  static std::string m_strip( const std::string& name )
  {
    const char* ws = " \t\n\r\f\v";
    std::size_t first = name.find_first_not_of(ws);
    if (first == std::string::npos)
      return "";
    std::size_t last = name.find_last_not_of(ws);
    std::string n = name.substr(first, last - first + 1);
    std::size_t scope = n.rfind("::");
    if (scope != std::string::npos)
      n = n.substr(scope + 2);
    return n;
  }
}; // class uvm_enum_wrapper

// names of the UVM enumeration types
template <> void uvm_enum_wrapper<uvm_active_passive_enum>::m_init_map();
template <> void uvm_enum_wrapper<uvm_verbosity>::m_init_map();
template <> void uvm_enum_wrapper<uvm_severity>::m_init_map();
template <> void uvm_enum_wrapper<uvm_sequencer_arb_mode>::m_init_map();
template <> void uvm_enum_wrapper<uvm_phase_state>::m_init_map();
template <> void uvm_enum_wrapper<uvm_radix_enum>::m_init_map();

//----------------------------------------------------------------------
// Class: uvm_enum_registrar<T>
//
//! Implementation defined
//! Registers the names of the values of an enumeration type with
//! uvm_enum_wrapper<T> when it is constructed. Used by the macro
//! UVM_ENUM_REGISTER.
//----------------------------------------------------------------------

template <typename T>
struct uvm_enum_registrar
{
  uvm_enum_registrar( const char* names, std::initializer_list<T> values )
  {
    uvm_enum_wrapper<T>::m_add_names(names, values);
  }
};

} // namespace uvm

//----------------------------------------------------------------------
// MACRO: UVM_ENUM_REGISTER
//
//! Registers the enumerators given after the enumeration type T with
//! uvm_enum_wrapper<T>, under their own names, without qualifiers.
//! The macro can be used at namespace scope or in a function body, also
//! several times on one line or on the same line of different headers.
//----------------------------------------------------------------------

// the name of the registrar object is unique in the translation unit
// with __COUNTER__, which GCC, Clang and MSVC provide
#ifdef __COUNTER__
#define UVM_ENUM_REGISTER_ID_ __COUNTER__
#else
#define UVM_ENUM_REGISTER_ID_ __LINE__
#endif

#define UVM_ENUM_REGISTER(T, ...) \
  static ::uvm::uvm_enum_registrar<T> \
    UVM_ENUM_REGISTER_NAME_(uvm_enum_registrar_, UVM_ENUM_REGISTER_ID_)(#__VA_ARGS__, { __VA_ARGS__ });

#define UVM_ENUM_REGISTER_NAME_(A, B) UVM_ENUM_REGISTER_CONCAT_(A, B)
#define UVM_ENUM_REGISTER_CONCAT_(A, B) A##B

#endif // UVM_ENUM_WRAPPER_H_
