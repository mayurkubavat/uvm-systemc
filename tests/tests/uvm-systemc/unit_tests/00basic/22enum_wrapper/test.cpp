//----------------------------------------------------------------------
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

// Test of uvm_enum_wrapper<T>::from_name: names registered with the
// macro UVM_ENUM_REGISTER and with add, for unscoped and scoped
// enumerations, and the names of the UVM enumeration types.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

namespace my_pkg {
  enum mode_e { MODE_IDLE, MODE_RUN = 5, MODE_STOP = 10 };
}

enum class color_e { RED, GREEN, BLUE };

enum class unknown_e { ONE, TWO };

// registration at namespace scope, with qualified names
UVM_ENUM_REGISTER(my_pkg::mode_e, my_pkg::MODE_IDLE, my_pkg::MODE_RUN, my_pkg::MODE_STOP)
UVM_ENUM_REGISTER(color_e, color_e::RED, color_e::GREEN, color_e::BLUE)

template <typename T>
void check( const std::string& type, const std::string& name, T init )
{
  T value = init;
  bool found = uvm_enum_wrapper<T>::from_name(name, value);
  std::cout << type << " \"" << name << "\": "
            << (found ? "found " : "not found, unchanged ") << static_cast<int>(value)
            << std::endl;
}

int sc_main(int, char*[])
{
  check<my_pkg::mode_e>("mode_e", "MODE_IDLE", my_pkg::MODE_STOP);
  check<my_pkg::mode_e>("mode_e", "MODE_RUN", my_pkg::MODE_IDLE);
  check<my_pkg::mode_e>("mode_e", "MODE_STOP", my_pkg::MODE_IDLE);
  check<my_pkg::mode_e>("mode_e", "my_pkg::MODE_RUN", my_pkg::MODE_IDLE);
  check<my_pkg::mode_e>("mode_e", "mode_run", my_pkg::MODE_IDLE);

  check<color_e>("color_e", "RED", color_e::BLUE);
  check<color_e>("color_e", "GREEN", color_e::RED);
  check<color_e>("color_e", "BLUE", color_e::RED);
  check<color_e>("color_e", "", color_e::GREEN);

  // no names registered for this type
  check<unknown_e>("unknown_e", "ONE", unknown_e::TWO);

  // registration in a function body
  UVM_ENUM_REGISTER(unknown_e, unknown_e::ONE, unknown_e::TWO)
  check<unknown_e>("unknown_e", "ONE", unknown_e::TWO);

  // additional names with add; a name keeps its value
  std::cout << "add green: " << uvm_enum_wrapper<color_e>::add("green", color_e::GREEN) << std::endl;
  std::cout << "add GREEN again: " << uvm_enum_wrapper<color_e>::add("GREEN", color_e::GREEN) << std::endl;
  std::cout << "add GREEN as BLUE: " << uvm_enum_wrapper<color_e>::add("GREEN", color_e::BLUE) << std::endl;
  check<color_e>("color_e", "green", color_e::RED);
  check<color_e>("color_e", "GREEN", color_e::RED);

  // the UVM enumeration types
  check<uvm_active_passive_enum>("uvm_active_passive_enum", "UVM_PASSIVE", UVM_ACTIVE);
  check<uvm_active_passive_enum>("uvm_active_passive_enum", "UVM_ACTIVE", UVM_PASSIVE);
  check<uvm_verbosity>("uvm_verbosity", "UVM_HIGH", UVM_NONE);
  check<uvm_verbosity>("uvm_verbosity", "UVM_DEBUG", UVM_NONE);
  check<uvm_severity>("uvm_severity", "UVM_ERROR", UVM_INFO);
  check<uvm_sequencer_arb_mode>("uvm_sequencer_arb_mode", "UVM_SEQ_ARB_RANDOM", SEQ_ARB_FIFO);
  check<uvm_sequencer_arb_mode>("uvm_sequencer_arb_mode", "SEQ_ARB_USER", SEQ_ARB_FIFO);
  check<uvm_phase_state>("uvm_phase_state", "UVM_PHASE_JUMPING", UVM_PHASE_DORMANT);
  check<uvm_radix_enum>("uvm_radix_enum", "UVM_HEX", UVM_BIN);
  check<uvm_radix_enum>("uvm_radix_enum", "UVM_FOO", UVM_BIN);

  // the default template argument
  uvm_active_passive_enum ap = UVM_ACTIVE;
  std::cout << "default type: " << uvm_enum_wrapper<>::from_name("UVM_PASSIVE", ap)
            << " " << ap << std::endl;

  return 0;
}
