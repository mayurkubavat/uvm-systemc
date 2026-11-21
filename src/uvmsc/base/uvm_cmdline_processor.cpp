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

#include <cctype>
#include <exception>

#include <systemc>

#include "uvmsc/base/uvm_cmdline_processor.h"
#include "uvmsc/base/uvm_globals.h"

namespace uvm {

//----------------------------------------------------------------------
// member function: get_inst (static)
//
//! Returns the singleton instance of the UVM command line processor.
//----------------------------------------------------------------------

uvm_cmdline_processor* uvm_cmdline_processor::get_inst()
{
  static uvm_cmdline_processor* m_inst = nullptr;

  if (m_inst == nullptr)
    m_inst = new uvm_cmdline_processor("uvm_cmdline_proc");

  return m_inst;
}

//----------------------------------------------------------------------
// Group: Basic arguments
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// member function: get_args
//
//! Returns a vector with all of the command line arguments that were
//! used to start the simulation. Element 0 of the vector is the name
//! of the executable which started the simulation.
//----------------------------------------------------------------------

void uvm_cmdline_processor::get_args( std::vector<std::string>& args )
{
  m_load_args();
  args = m_argv;
}

//----------------------------------------------------------------------
// member function: get_plusargs
//
//! Returns a vector with all of the plus arguments that were used to
//! start the simulation. A plus argument is an argument which starts
//! with a +.
//----------------------------------------------------------------------

void uvm_cmdline_processor::get_plusargs( std::vector<std::string>& args )
{
  m_load_args();
  args = m_plus_argv;
}

//----------------------------------------------------------------------
// member function: get_uvm_args
//
//! Returns a vector with all of the UVM arguments that were used to
//! start the simulation. A UVM argument is any argument that starts
//! with a - or + and uses the keyword UVM (case insensitive) as the
//! first three letters of the argument.
//----------------------------------------------------------------------

void uvm_cmdline_processor::get_uvm_args( std::vector<std::string>& args )
{
  m_load_args();
  args = m_uvm_argv;
}

//----------------------------------------------------------------------
// member function: get_arg_matches
//
//! Loads \p args with all of the arguments that match the expression
//! \p match and returns the number of items that matched. If the
//! expression is bracketed with //, it is taken as an extended regular
//! expression which the argument has to match, as for the function
//! uvm_is_match. Otherwise, it is taken as the beginning of an argument
//! to match.
//----------------------------------------------------------------------

int uvm_cmdline_processor::get_arg_matches( const std::string& match,
                                            std::vector<std::string>& args )
{
  bool match_is_regex = (match.size() > 2) && (match[0] == '/') &&
                        (match[match.size()-1] == '/');

  m_load_args();
  args.clear();

  for (unsigned int i = 0; i < m_argv.size(); i++)
  {
    if (match_is_regex)
    {
      try
      {
        if (uvm_is_match(match, m_argv[i]))
          args.push_back(m_argv[i]);
      }
      catch (const std::exception&)
      {
        uvm_report_warning("UVM_CMDLINE_PROC",
          "Invalid regular expression '" + match + "' for get_arg_matches");
        break;
      }
    }
    else if (m_argv[i].compare(0, match.size(), match) == 0)
      args.push_back(m_argv[i]);
  }

  return args.size();
}

//----------------------------------------------------------------------
// Group: Argument values
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// member function: get_arg_value
//
//! Finds the first argument which starts with \p match and returns the
//! remainder of that argument in \p value. The return value is the
//! number of command line arguments that start with \p match.
//----------------------------------------------------------------------

int uvm_cmdline_processor::get_arg_value( const std::string& match,
                                          std::string& value )
{
  int count = 0;

  m_load_args();

  for (unsigned int i = 0; i < m_argv.size(); i++)
    if (m_argv[i].compare(0, match.size(), match) == 0)
    {
      count++;
      if (count == 1)
        value = m_argv[i].substr(match.size());
    }

  return count;
}

//----------------------------------------------------------------------
// member function: get_arg_values
//
//! Finds all the arguments which start with \p match and returns the
//! remainder of each of these arguments in \p values. The return value
//! is the number of matches that were found.
//----------------------------------------------------------------------

int uvm_cmdline_processor::get_arg_values( const std::string& match,
                                           std::vector<std::string>& values )
{
  m_load_args();
  values.clear();

  for (unsigned int i = 0; i < m_argv.size(); i++)
    if (m_argv[i].compare(0, match.size(), match) == 0)
      values.push_back(m_argv[i].substr(match.size()));

  return values.size();
}

//----------------------------------------------------------------------
// Group: Tool information
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// member function: get_tool_name
//
//! Returns the simulation tool that is executing the simulation.
//----------------------------------------------------------------------

std::string uvm_cmdline_processor::get_tool_name() const
{
  return "SystemC";
}

//----------------------------------------------------------------------
// member function: get_tool_version
//
//! Returns the version of the simulation tool that is executing the
//! simulation.
//----------------------------------------------------------------------

std::string uvm_cmdline_processor::get_tool_version() const
{
  return sc_core::sc_release();
}

////////////////////////////////////////////////////////////////////////
//////// Implementation-defined member functions start here ////////////
////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------

uvm_cmdline_processor::uvm_cmdline_processor( const std::string& name )
: uvm_report_object(name), m_args_loaded(false)
{
  start_report_handler(name);
  m_load_args();
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------

uvm_cmdline_processor::~uvm_cmdline_processor()
{
}

//----------------------------------------------------------------------
// member function: m_load_args
//
//! Implementation defined
//! Reads the command line arguments from SystemC. The arguments are
//! not available before SystemC has called sc_main, so loading them is
//! retried until then.
//----------------------------------------------------------------------

void uvm_cmdline_processor::m_load_args()
{
  if (m_args_loaded)
    return;

  int argc = sc_core::sc_argc();
  const char* const* argv = sc_core::sc_argv();

  if (argc <= 0 || argv == nullptr)
    return;

  for (int i = 0; i < argc; i++)
  {
    std::string s = (argv[i] != nullptr) ? argv[i] : "";

    if (s.empty())
      continue;

    m_argv.push_back(s);

    if (s[0] == '+')
      m_plus_argv.push_back(s);

    if (s.size() >= 4 && (s[0] == '-' || s[0] == '+'))
    {
      std::string sub = s.substr(1, 3);
      for (unsigned int j = 0; j < sub.size(); j++)
        sub[j] = std::toupper(static_cast<unsigned char>(sub[j]));
      if (sub == "UVM")
        m_uvm_argv.push_back(s);
    }
  }

  m_args_loaded = true;
}

} // namespace uvm
