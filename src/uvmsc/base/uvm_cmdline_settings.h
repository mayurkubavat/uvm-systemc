//----------------------------------------------------------------------
//   Copyright 2022 AMD
//   Copyright 2007-2024 Cadence Design Systems, Inc.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2020-2024 NVIDIA Corporation
//   Copyright 2010-2011 Synopsys, Inc.
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

#ifndef UVM_CMDLINE_SETTINGS_H_
#define UVM_CMDLINE_SETTINGS_H_

#include <string>

namespace uvm {

// forward class declarations
class uvm_root;
class uvm_component;

//----------------------------------------------------------------------
// CLASS: uvm_cmdline_settings
//
//! Implementation defined class, not part of the UVM Class reference.
//! Applies the UVM built-in command line arguments, see the class
//! uvm_cmdline_processor for the list of arguments.
//----------------------------------------------------------------------

class uvm_cmdline_settings
{
 public:

  // Parse the report related arguments and apply the arguments which
  // have to take effect before any component exists. Does not report,
  // so it can be called while uvm_root is being constructed. Does
  // nothing when the command line arguments are not available yet or
  // when it has been called before.
  static void init( uvm_root* top );

  // Apply the arguments which take effect when a test is started.
  // To be called by uvm_root::run_test only.
  static void apply( uvm_root* top );

  // Return the test given by +UVM_TESTNAME, or test_name if there is
  // no such argument.
  static std::string get_test_name( uvm_root* top,
                                    const std::string& test_name );

  // Return true if the command line contains an argument which starts
  // with the string match. Returns false before init has been done.
  static bool has_arg( const std::string& match );

  // Apply +uvm_set_action, +uvm_set_severity and the +uvm_set_verbosity
  // arguments which do not wait for a phase or time to a component
  // which has just been constructed.
  static void apply_report_settings( uvm_component* comp );

  // Apply the +uvm_set_verbosity arguments for the phase phase_name to
  // a component.
  static void apply_verbosity_settings( uvm_component* comp,
                                        const std::string& phase_name );

 private:
  // disabled
  uvm_cmdline_settings();
};

} // namespace uvm

#endif /* UVM_CMDLINE_SETTINGS_H_ */
