//----------------------------------------------------------------------
//   Copyright 2010-2018 AMD
//   Copyright 2015 Analog Devices, Inc.
//   Copyright 2010-2018 Cadence Design Systems, Inc.
//   Copyright 2017 Cisco Systems, Inc.
//   Copyright 2021-2023 Marvell International Ltd.
//   Copyright 2010-2018 Mentor Graphics Corporation
//   Copyright 2013-2024 NVIDIA Corporation
//   Copyright 2011 Synopsys, Inc.
//   Copyright 2020 Verific
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

#ifndef UVM_CMDLINE_PROCESSOR_H_
#define UVM_CMDLINE_PROCESSOR_H_

#include <string>
#include <vector>

#include "uvmsc/report/uvm_report_object.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_cmdline_processor
//
//! The class uvm_cmdline_processor provides an interface to the command
//! line arguments which were used to start the simulation. The
//! arguments are those passed to sc_main, as made available by
//! the functions sc_core::sc_argc and sc_core::sc_argv.
//!
//! The class is also used to set up the UVM built-in command line
//! arguments, which all start with +UVM (or +uvm):
//!
//! +UVM_TESTNAME=<name>             test to create in uvm_root::run_test
//! +UVM_VERBOSITY=<verbosity>       initial verbosity of all components
//! +UVM_TIMEOUT=<timeout>,<YES|NO>  global timeout and whether it can
//!                                  be overridden by the application.
//!                                  The timeout is a number with an
//!                                  optional unit (fs, ps, ns, us, ms
//!                                  or s). Without unit, nanoseconds
//!                                  are used.
//! +UVM_MAX_QUIT_COUNT=<count>,<YES|NO>
//! +UVM_SEED=<seed>|random          global seed, see uvm_set_random_seed.
//!                                  With "random" a new seed is used in
//!                                  each run; it is reported so that
//!                                  the run can be repeated.
//! +UVM_PHASE_TRACE
//! +UVM_OBJECTION_TRACE
//! +UVM_CONFIG_DB_TRACE
//! +UVM_RESOURCE_DB_TRACE
//! +UVM_DUMP_CMDLINE_ARGS
//! +uvm_set_verbosity=<comp>,<id>,<verbosity>,<phase>
//! +uvm_set_verbosity=<comp>,<id>,<verbosity>,time,<time>
//! +uvm_set_action=<comp>,<id>,<severity>,<action>
//! +uvm_set_severity=<comp>,<id>,<current severity>,<new severity>
//! +uvm_set_inst_override=<req_type>,<override_type>,<full_inst_path>
//! +uvm_set_type_override=<req_type>,<override_type>[,<replace>]
//! +uvm_set_config_int=<comp>,<field>,<value>
//! +uvm_set_config_bitstream=<comp>,<field>,<value>
//! +uvm_set_config_string=<comp>,<field>,<value>
//! +uvm_set_default_sequence=<seqr>,<phase>,<type>
//!
//! A value set with +uvm_set_config_int is stored for both the types
//! uvm_bitstream_t (uvm_config_int) and int, so it is found by
//! uvm_config_db<int>::get as well.
//! A value set with +uvm_set_config_bitstream is stored for the type
//! uvm_bitstream_t only. Values of up to 64 bits can be given.
//----------------------------------------------------------------------

class uvm_cmdline_processor : public uvm_report_object
{
 public:

  //--------------------------------------------------------------------
  // Group: Singleton
  //--------------------------------------------------------------------

  static uvm_cmdline_processor* get_inst();

  //--------------------------------------------------------------------
  // Group: Basic arguments
  //--------------------------------------------------------------------

  void get_args( std::vector<std::string>& args );

  void get_plusargs( std::vector<std::string>& args );

  void get_uvm_args( std::vector<std::string>& args );

  int get_arg_matches( const std::string& match,
                       std::vector<std::string>& args );

  //--------------------------------------------------------------------
  // Group: Argument values
  //--------------------------------------------------------------------

  int get_arg_value( const std::string& match, std::string& value );

  int get_arg_values( const std::string& match,
                      std::vector<std::string>& values );

  //--------------------------------------------------------------------
  // Group: Tool information
  //--------------------------------------------------------------------

  std::string get_tool_name() const;

  std::string get_tool_version() const;

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

 protected:

  explicit uvm_cmdline_processor( const std::string& name = "" );

  virtual ~uvm_cmdline_processor();

  std::vector<std::string> m_argv;
  std::vector<std::string> m_plus_argv;
  std::vector<std::string> m_uvm_argv;

 private:

  void m_load_args();

  bool m_args_loaded;

  // disabled
  uvm_cmdline_processor( const uvm_cmdline_processor& );
  uvm_cmdline_processor& operator=( const uvm_cmdline_processor& );
};

} // namespace uvm

#endif /* UVM_CMDLINE_PROCESSOR_H_ */
