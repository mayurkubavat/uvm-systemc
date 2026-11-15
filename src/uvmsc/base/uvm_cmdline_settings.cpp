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

#ifndef SC_INCLUDE_DYNAMIC_PROCESSES
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>

#include "uvmsc/base/uvm_cmdline_settings.h"
#include "uvmsc/base/uvm_cmdline_processor.h"
#include "uvmsc/base/uvm_component.h"
#include "uvmsc/base/uvm_coreservice_t.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/base/uvm_object_globals.h"
#include "uvmsc/base/uvm_root.h"
#include "uvmsc/conf/uvm_config_db.h"
#include "uvmsc/conf/uvm_config_db_options.h"
#include "uvmsc/conf/uvm_resource_db_options.h"
#include "uvmsc/factory/uvm_factory.h"
#include "uvmsc/factory/uvm_object_wrapper.h"
#include "uvmsc/report/uvm_report_server.h"

namespace uvm {

namespace {

//----------------------------------------------------------------------
// Parsed settings
//----------------------------------------------------------------------

struct verbosity_setting
{
  std::string comp;
  std::string id;
  int verbosity;
  std::string phase;
  sc_core::sc_time offset;
};

struct action_setting
{
  std::string comp;
  std::string id;
  bool all_sev;
  uvm_severity sev;
  uvm_action action;
};

struct severity_setting
{
  std::string comp;
  std::string id;
  bool all_sev;
  uvm_severity orig_sev;
  uvm_severity sev;
};

// report which could not be issued when the arguments were parsed
struct pending_report
{
  uvm_severity severity;
  std::string id;
  std::string message;
};

struct settings_t
{
  settings_t() : initialized(false) {}

  bool initialized;
  std::vector<verbosity_setting> verbosity;
  std::vector<action_setting> action;
  std::vector<severity_setting> severity;
  std::vector<pending_report> reports;
};

settings_t& settings()
{
  static settings_t s;
  return s;
}

const uvm_severity all_severities[] =
  { UVM_INFO, UVM_WARNING, UVM_ERROR, UVM_FATAL };

//----------------------------------------------------------------------
// String helpers
//----------------------------------------------------------------------

std::vector<std::string> split( const std::string& str, char sep = ',' )
{
  std::vector<std::string> values;
  std::string::size_type start = 0;
  std::string::size_type pos;

  while ((pos = str.find(sep, start)) != std::string::npos)
  {
    values.push_back(str.substr(start, pos - start));
    start = pos + 1;
  }
  values.push_back(str.substr(start));

  return values;
}

std::string join( const std::vector<std::string>& values,
                  const std::string& sep )
{
  std::string str;

  for (unsigned int i = 0; i < values.size(); i++)
  {
    if (i != 0) str += sep;
    str += values[i];
  }
  return str;
}

bool string_to_int( const std::string& str, int& value )
{
  char* end = nullptr;
  errno = 0;
  long v = std::strtol(str.c_str(), &end, 10);

  if (str.empty() || *end != '\0' || errno != 0 || v < INT_MIN || v > INT_MAX)
    return false;

  value = static_cast<int>(v);
  return true;
}

bool string_to_verbosity( const std::string& str, int& verbosity )
{
  if      (str == "UVM_NONE"   || str == "NONE")   verbosity = UVM_NONE;
  else if (str == "UVM_LOW"    || str == "LOW")    verbosity = UVM_LOW;
  else if (str == "UVM_MEDIUM" || str == "MEDIUM") verbosity = UVM_MEDIUM;
  else if (str == "UVM_HIGH"   || str == "HIGH")   verbosity = UVM_HIGH;
  else if (str == "UVM_FULL"   || str == "FULL")   verbosity = UVM_FULL;
  else if (str == "UVM_DEBUG"  || str == "DEBUG")  verbosity = UVM_DEBUG;
  else return false;

  return true;
}

bool string_to_severity( const std::string& str, uvm_severity& sev )
{
  if      (str == "UVM_INFO")    sev = UVM_INFO;
  else if (str == "UVM_WARNING") sev = UVM_WARNING;
  else if (str == "UVM_ERROR")   sev = UVM_ERROR;
  else if (str == "UVM_FATAL")   sev = UVM_FATAL;
  else return false;

  return true;
}

bool string_to_action( const std::string& str, uvm_action& action )
{
  std::vector<std::string> actions = split(str, '|');

  action = UVM_NO_ACTION;

  for (unsigned int i = 0; i < actions.size(); i++)
  {
    if      (actions[i] == "UVM_NO_ACTION") action |= UVM_NO_ACTION;
    else if (actions[i] == "UVM_DISPLAY")   action |= UVM_DISPLAY;
    else if (actions[i] == "UVM_LOG")       action |= UVM_LOG;
    else if (actions[i] == "UVM_COUNT")     action |= UVM_COUNT;
    else if (actions[i] == "UVM_EXIT")      action |= UVM_EXIT;
    else if (actions[i] == "UVM_CALL_HOOK") action |= UVM_CALL_HOOK;
    else if (actions[i] == "UVM_STOP")      action |= UVM_STOP;
    else if (actions[i] == "UVM_RM_RECORD") action |= UVM_RM_RECORD;
    else return false;
  }
  return true;
}

// Convert a number with an optional time unit. Without unit, the
// number is taken in nanoseconds.
bool string_to_time( const std::string& str, sc_core::sc_time& t )
{
  // a decimal number: digits with an optional fraction
  std::string::size_type pos = 0;
  while (pos < str.size() &&
         (std::isdigit(static_cast<unsigned char>(str[pos])) || str[pos] == '.'))
    pos++;

  if (pos == 0)
    return false;

  char* end = nullptr;
  errno = 0;
  double v = std::strtod(str.substr(0, pos).c_str(), &end);

  if (*end != '\0' || errno != 0)
    return false;

  std::string unit = str.substr(pos);
  double seconds;

  if      (unit.empty() || unit == "ns") { t = sc_core::sc_time(v, sc_core::SC_NS); seconds = v * 1e-9; }
  else if (unit == "fs") { t = sc_core::sc_time(v, sc_core::SC_FS);  seconds = v * 1e-15; }
  else if (unit == "ps") { t = sc_core::sc_time(v, sc_core::SC_PS);  seconds = v * 1e-12; }
  else if (unit == "us") { t = sc_core::sc_time(v, sc_core::SC_US);  seconds = v * 1e-6; }
  else if (unit == "ms") { t = sc_core::sc_time(v, sc_core::SC_MS);  seconds = v * 1e-3; }
  else if (unit == "s")  { t = sc_core::sc_time(v, sc_core::SC_SEC); seconds = v; }
  else return false;

  // the time has to be representable: not beyond the maximum time, and
  // not rounded to zero by the time resolution
  if (seconds > sc_core::sc_max_time().to_seconds())
    return false;

  if (v > 0.0 && t == sc_core::SC_ZERO_TIME)
    return false;

  return true;
}

// Convert an integral value with an optional radix prefix
// ('b, 0b, 'o, 'd, 'h, 'x or 0x).
bool string_to_integral( const std::string& str, uvm_integral_t& value )
{
  std::string digits = str;
  int base = 10;

  if (str.size() > 2)
  {
    std::string prefix = str.substr(0, 2);

    if      (prefix == "'b" || prefix == "0b") base = 2;
    else if (prefix == "'o")                   base = 8;
    else if (prefix == "'d")                   base = 10;
    else if (prefix == "'h" || prefix == "'x" || prefix == "0x") base = 16;

    if (base != 10 || prefix == "'d")
      digits = str.substr(2);
  }

  if (digits.empty())
    return false;

  char* end = nullptr;
  errno = 0;

  if (base == 10)
  {
    long long v = std::strtoll(digits.c_str(), &end, 10);
    value = static_cast<sc_dt::int64>(v);
  }
  else
  {
    if (digits[0] == '-' || digits[0] == '+')
      return false;
    unsigned long long v = std::strtoull(digits.c_str(), &end, base);
    value = static_cast<sc_dt::uint64>(v);
  }

  return (*end == '\0' && errno == 0);
}

// Return the values of all arguments which start with one of the two
// given prefixes, in the order in which they are on the command line.
std::vector<std::string> get_values( const std::string& prefix1,
                                     const std::string& prefix2 = "" )
{
  std::vector<std::string> args;
  std::vector<std::string> values;

  uvm_cmdline_processor::get_inst()->get_args(args);

  for (unsigned int i = 0; i < args.size(); i++)
  {
    if (args[i].compare(0, prefix1.size(), prefix1) == 0)
      values.push_back(args[i].substr(prefix1.size()));
    else if (!prefix2.empty() &&
             args[i].compare(0, prefix2.size(), prefix2) == 0)
      values.push_back(args[i].substr(prefix2.size()));
  }
  return values;
}

// Match the full name of a component against the pattern of a setting.
// An expression which is not valid does not match.
bool is_match( const std::string& pattern, const std::string& name )
{
  try
  {
    return uvm_is_match(pattern, name);
  }
  catch (const std::exception&)
  {
    return false;
  }
}

// Return true if the string can be used as a component pattern
bool is_valid_pattern( const std::string& pattern )
{
  if (pattern.empty())
    return false;

  try
  {
    uvm_is_match(pattern, "");
  }
  catch (const std::exception&)
  {
    return false;
  }
  return true;
}

void add_report( uvm_severity severity,
                 const std::string& id,
                 const std::string& message )
{
  pending_report r;
  r.severity = severity;
  r.id = id;
  r.message = message;
  settings().reports.push_back(r);
}

//----------------------------------------------------------------------
// Parsing of the report related arguments
//----------------------------------------------------------------------

// +UVM_VERBOSITY=<verbosity>
bool parse_verbosity( int& verbosity )
{
  std::vector<std::string> values = get_values("+UVM_VERBOSITY=");

  if (values.empty())
    return false;

  if (!string_to_verbosity(values[0], verbosity))
  {
    if (string_to_int(values[0], verbosity))
    {
      std::ostringstream str;
      str << "Non-standard verbosity value '" << values[0]
          << "', converted to '" << verbosity << "'.";
      add_report(UVM_INFO, "NSTVERB", str.str());
    }
    else
    {
      verbosity = UVM_MEDIUM;
      add_report(UVM_WARNING, "ILLVERB", "Illegal verbosity value '" +
        values[0] + "', converted to default of UVM_MEDIUM.");
    }
  }

  if (values.size() > 1)
  {
    std::ostringstream str;
    str << "Multiple (" << values.size()
        << ") +UVM_VERBOSITY arguments provided on the command line.  '"
        << values[0] << "' will be used.  Provided list: "
        << join(values, ", ") << ".";
    add_report(UVM_WARNING, "MULTVERB", str.str());
  }

  return true;
}

// +uvm_set_verbosity=<comp>,<id>,<verbosity>,<phase>
// +uvm_set_verbosity=<comp>,<id>,<verbosity>,time,<time>
void parse_set_verbosity()
{
  const std::string prefix = "+uvm_set_verbosity=";
  std::vector<std::string> values = get_values(prefix);

  for (unsigned int i = 0; i < values.size(); i++)
  {
    std::vector<std::string> args = split(values[i]);
    verbosity_setting setting;
    std::string message;

    if (args.size() < 4 || args.size() > 5)
      message = "Invalid number of arguments found, expected 4 or 5";
    else if (args.size() == 5 && args[3] != "time")
      message = "Too many arguments found for <phase>, expected only 4";
    else if (args.size() == 4 && args[3] == "time")
      message = "Too few arguments found for <time>, expected 5";
    else if (!string_to_verbosity(args[2], setting.verbosity))
      message = "Invalid verbosity found";
    else if (args.size() == 5 && !string_to_time(args[4], setting.offset))
      message = "Invalid time found";
    else if (!is_valid_pattern(args[0]))
      message = "Invalid component found";

    if (!message.empty())
    {
      add_report(UVM_WARNING, "INVLCMDARGS", message + ", setting '" +
        prefix + values[i] + "' will be ignored.");
      continue;
    }

    setting.comp = args[0];
    setting.id = args[1];
    setting.phase = args[3];
    settings().verbosity.push_back(setting);
  }
}

// +uvm_set_action=<comp>,<id>,<severity>,<action>
void parse_set_action()
{
  const std::string prefix = "+uvm_set_action=";
  std::vector<std::string> values = get_values(prefix);

  for (unsigned int i = 0; i < values.size(); i++)
  {
    std::vector<std::string> args = split(values[i]);
    action_setting setting;
    std::string message;

    setting.sev = UVM_INFO;

    if (args.size() != 4)
      message = "Invalid number of arguments found, expected 4";
    else if (args[2] != "_ALL_" && !string_to_severity(args[2], setting.sev))
      message = "Bad severity argument \"" + args[2] + "\"";
    else if (!string_to_action(args[3], setting.action))
      message = "Bad action argument \"" + args[3] + "\"";
    else if (!is_valid_pattern(args[0]))
      message = "Invalid component found";

    if (!message.empty())
    {
      add_report(UVM_WARNING, "INVLCMDARGS", message + ", setting '" +
        prefix + values[i] + "' will be ignored.");
      continue;
    }

    setting.comp = args[0];
    setting.id = args[1];
    setting.all_sev = (args[2] == "_ALL_");
    settings().action.push_back(setting);
  }
}

// +uvm_set_severity=<comp>,<id>,<current severity>,<new severity>
void parse_set_severity()
{
  const std::string prefix = "+uvm_set_severity=";
  std::vector<std::string> values = get_values(prefix);

  for (unsigned int i = 0; i < values.size(); i++)
  {
    std::vector<std::string> args = split(values[i]);
    severity_setting setting;
    std::string message;

    setting.orig_sev = UVM_INFO;

    if (args.size() != 4)
      message = "Invalid number of arguments found, expected 4";
    else if (args[2] != "_ALL_" && !string_to_severity(args[2], setting.orig_sev))
      message = "Bad severity argument \"" + args[2] + "\"";
    else if (!string_to_severity(args[3], setting.sev))
      message = "Bad severity argument \"" + args[3] + "\"";
    else if (!is_valid_pattern(args[0]))
      message = "Invalid component found";

    if (!message.empty())
    {
      add_report(UVM_WARNING, "INVLCMDARGS", message + ", setting '" +
        prefix + values[i] + "' will be ignored.");
      continue;
    }

    setting.comp = args[0];
    setting.id = args[1];
    setting.all_sev = (args[2] == "_ALL_");
    settings().severity.push_back(setting);
  }
}

//----------------------------------------------------------------------
// Application of the arguments
//----------------------------------------------------------------------

void set_verbosity( uvm_component* comp, const verbosity_setting& setting )
{
  if (setting.id == "_ALL_")
    comp->set_report_verbosity_level(setting.verbosity);
  else
    comp->set_report_id_verbosity(setting.id, setting.verbosity);
}

bool time_setting_less( const verbosity_setting& a, const verbosity_setting& b )
{
  return a.offset < b.offset;
}

// Process which applies the +uvm_set_verbosity arguments with a time
void time_verbosity_process( std::vector<verbosity_setting> time_settings )
{
  uvm_root* top = uvm_coreservice_t::get()->get_root();

  for (unsigned int i = 0; i < time_settings.size(); i++)
  {
    std::vector<uvm_component*> comps;

    if (time_settings[i].offset > sc_core::sc_time_stamp())
      sc_core::wait(time_settings[i].offset - sc_core::sc_time_stamp());

    top->find_all(time_settings[i].comp, comps);

    for (unsigned int j = 0; j < comps.size(); j++)
      set_verbosity(comps[j], time_settings[i]);
  }
}

void do_dump_args( uvm_root* top )
{
  std::vector<std::string> args;

  if (!uvm_cmdline_settings::has_arg("+UVM_DUMP_CMDLINE_ARGS"))
    return;

  uvm_cmdline_processor::get_inst()->get_args(args);

  for (unsigned int i = 0; i < args.size(); i++)
  {
    std::ostringstream str;
    str << "idx=" << i << " arg=[" << args[i] << "]";
    top->uvm_report_info("DUMPARGS", str.str(), UVM_NONE);
  }
}

void do_time_verbosity_settings()
{
  std::vector<verbosity_setting> time_settings;
  const std::vector<verbosity_setting>& all = settings().verbosity;

  for (unsigned int i = 0; i < all.size(); i++)
    if (all[i].phase == "time" && all[i].offset != sc_core::SC_ZERO_TIME)
      time_settings.push_back(all[i]);

  if (time_settings.empty())
    return;

  std::stable_sort(time_settings.begin(), time_settings.end(), time_setting_less);

  sc_core::sc_spawn(sc_bind(&time_verbosity_process, time_settings),
    sc_core::sc_gen_unique_name("m_cmdline_verbosity_proc"));
}

// Split the second field of a <value>,<YES|NO> argument
bool is_overridable( const std::vector<std::string>& args )
{
  return !(args.size() > 1 && args[1] == "NO");
}

// +UVM_TIMEOUT=<timeout>,<overridable>
void do_timeout_settings( uvm_root* top )
{
  std::vector<std::string> values = get_values("+UVM_TIMEOUT=");
  sc_core::sc_time timeout;

  if (values.empty())
    return;

  if (values.size() > 1)
  {
    std::ostringstream str;
    str << "Multiple (" << values.size()
        << ") +UVM_TIMEOUT arguments provided on the command line.  '"
        << values[0] << "' will be used.  Provided list: "
        << join(values, "; ") << ".";
    top->uvm_report_warning("MULTTIMOUT", str.str(), UVM_NONE);
  }

  std::vector<std::string> args = split(values[0]);

  if (!string_to_time(args[0], timeout))
  {
    top->uvm_report_error("UVM_CMDLINE_PROC", "Invalid timeout for +UVM_TIMEOUT=" +
      values[0] + ", setting will be ignored.", UVM_NONE);
    return;
  }

  top->uvm_report_info("TIMOUTSET", "'+UVM_TIMEOUT=" + values[0] +
    "' provided on the command line is being applied.", UVM_NONE);

  top->set_timeout(timeout, is_overridable(args));
}

// +UVM_MAX_QUIT_COUNT=<count>,<overridable>
void do_max_quit_settings( uvm_root* top )
{
  std::vector<std::string> values = get_values("+UVM_MAX_QUIT_COUNT=");
  int max_quit_count = 0;

  if (values.empty())
    return;

  if (values.size() > 1)
  {
    std::ostringstream str;
    str << "Multiple (" << values.size()
        << ") +UVM_MAX_QUIT_COUNT arguments provided on the command line.  '"
        << values[0] << "' will be used.  Provided list: "
        << join(values, "; ") << ".";
    top->uvm_report_warning("MULTMAXQUIT", str.str(), UVM_NONE);
  }

  std::vector<std::string> args = split(values[0]);

  if (!string_to_int(args[0], max_quit_count))
  {
    top->uvm_report_error("UVM_CMDLINE_PROC", "Invalid count for +UVM_MAX_QUIT_COUNT=" +
      values[0] + ", setting will be ignored.", UVM_NONE);
    return;
  }

  top->uvm_report_info("MAXQUITSET", "'+UVM_MAX_QUIT_COUNT=" + values[0] +
    "' provided on the command line is being applied.", UVM_NONE);

  uvm_report_server::get_server()->set_max_quit_count(max_quit_count,
    is_overridable(args));
}

// +uvm_set_inst_override=<req_type>,<override_type>,<full_inst_path>
// +uvm_set_type_override=<req_type>,<override_type>[,<replace>]
void do_factory_settings( uvm_root* top )
{
  std::vector<std::string> values;

  values = get_values("+uvm_set_inst_override=", "+UVM_SET_INST_OVERRIDE=");

  for (unsigned int i = 0; i < values.size(); i++)
  {
    std::vector<std::string> args = split(values[i]);

    if (args.size() != 3 || args[0].empty() || args[1].empty() || args[2].empty())
    {
      top->uvm_report_error("UVM_CMDLINE_PROC",
        "Invalid setting for +uvm_set_inst_override=" + values[i] +
        ", setting must specify <requested_type>,<override_type>,<instance_path>",
        UVM_NONE);
      continue;
    }

    top->uvm_report_info("INSTOVR",
      "Applying instance override from the command line: +uvm_set_inst_override=" +
      values[i], UVM_NONE);
    uvm_coreservice_t::get()->get_factory()->set_inst_override_by_name(
      args[0], args[1], args[2]);
  }

  values = get_values("+uvm_set_type_override=", "+UVM_SET_TYPE_OVERRIDE=");

  for (unsigned int i = 0; i < values.size(); i++)
  {
    std::vector<std::string> args = split(values[i]);
    bool replace = true;

    if (args.size() > 3 || args.size() < 2 || args[0].empty() || args[1].empty())
    {
      top->uvm_report_error("UVM_CMDLINE_PROC",
        "Invalid setting for +uvm_set_type_override=" + values[i] +
        ", setting must specify <requested_type>,<override_type>[,<replace>]",
        UVM_NONE);
      continue;
    }

    // The replace argument is optional. If set, it must be 0 or 1
    if (args.size() == 3)
    {
      if (args[2] == "0")
        replace = false;
      else if (args[2] == "1")
        replace = true;
      else
      {
        top->uvm_report_error("UVM_CMDLINE_PROC",
          "Invalid replace arg for +uvm_set_type_override=" + values[i] +
          " value must be 0 or 1", UVM_NONE);
        continue;
      }
    }

    top->uvm_report_info("UVM_CMDLINE_PROC",
      "Applying type override from the command line: +uvm_set_type_override=" +
      values[i], UVM_NONE);
    uvm_coreservice_t::get()->get_factory()->set_type_override_by_name(
      args[0], args[1], replace);
  }
}

// Check the number of fields of a +uvm_set_config_* argument
bool check_config_fields( uvm_root* top,
                          const std::string& cfg,
                          const std::vector<std::string>& args )
{
  if (args.size() == 1)
  {
    top->uvm_report_error("UVM_CMDLINE_PROC",
      "Invalid +uvm_set_config command\"" + cfg +
      "\" missing field and value: component is \"" + args[0] + "\"", UVM_NONE);
    return false;
  }

  if (args.size() == 2)
  {
    top->uvm_report_error("UVM_CMDLINE_PROC",
      "Invalid +uvm_set_config command\"" + cfg +
      "\" missing value: component is \"" + args[0] + "\"  field is \"" +
      args[1] + "\"", UVM_NONE);
    return false;
  }

  if (args.size() > 3)
  {
    top->uvm_report_error("UVM_CMDLINE_PROC",
      "Invalid +uvm_set_config command\"" + cfg +
      "\" : expected only 3 fields (component, field and value).", UVM_NONE);
    return false;
  }

  return true;
}

// +uvm_set_config_int=<comp>,<field>,<value>
// +uvm_set_config_string=<comp>,<field>,<value>
// +uvm_set_default_sequence=<seqr>,<phase>,<type>
void do_config_settings( uvm_root* top )
{
  std::vector<std::string> values;

  values = get_values("+uvm_set_config_int=", "+UVM_SET_CONFIG_INT=");

  for (unsigned int i = 0; i < values.size(); i++)
  {
    std::vector<std::string> args = split(values[i]);
    uvm_integral_t value;

    if (!check_config_fields(top, values[i], args))
      continue;

    if (!string_to_integral(args[2], value))
    {
      top->uvm_report_error("UVM_CMDLINE_PROC",
        "Invalid +uvm_set_config command\"" + values[i] +
        "\" : value \"" + args[2] + "\" is not an integral value.", UVM_NONE);
      continue;
    }

    top->uvm_report_info("UVM_CMDLINE_PROC",
      "Applying config setting from the command line: +uvm_set_config_int=" +
      values[i], UVM_NONE);

    uvm_config_int::set(top, args[0], args[1], uvm_bitstream_t(value.to_uint64()));
    // the value is also stored as int when it fits
    if (value.to_int64() >= INT_MIN && value.to_int64() <= INT_MAX)
      uvm_config_db<int>::set(top, args[0], args[1], static_cast<int>(value.to_int64()));
  }

  values = get_values("+uvm_set_config_string=", "+UVM_SET_CONFIG_STRING=");

  for (unsigned int i = 0; i < values.size(); i++)
  {
    std::vector<std::string> args = split(values[i]);

    if (!check_config_fields(top, values[i], args))
      continue;

    top->uvm_report_info("UVM_CMDLINE_PROC",
      "Applying config setting from the command line: +uvm_set_config_string=" +
      values[i], UVM_NONE);

    uvm_config_string::set(top, args[0], args[1], args[2]);
  }

  values = get_values("+uvm_set_default_sequence=", "+UVM_SET_DEFAULT_SEQUENCE=");

  for (unsigned int i = 0; i < values.size(); i++)
  {
    std::vector<std::string> args = split(values[i]);

    if (args.size() != 3)
    {
      top->uvm_report_error("UVM_CMDLINE_PROC",
        "Invalid +uvm_set_default_sequence command\"" + values[i] +
        "\" : expected 3 fields (sequencer, phase and type).", UVM_NONE);
      continue;
    }

    uvm_object_wrapper* wrapper =
      uvm_coreservice_t::get()->get_factory()->find_wrapper_by_name(args[2]);

    if (wrapper == nullptr)
    {
      top->uvm_report_error("UVM_CMDLINE_PROC", "Invalid type '" + args[2] +
        "' provided to +uvm_set_default_sequence", UVM_NONE);
      continue;
    }

    top->uvm_report_info("UVM_CMDLINE_PROC",
      "Setting default sequence from the command line: +uvm_set_default_sequence=" +
      values[i], UVM_NONE);

    uvm_config_db<uvm_object_wrapper*>::set(top, args[0] + "." + args[1],
      "default_sequence", wrapper);
  }
}

// Return true if the command line has an argument which starts with
// +uvm or -uvm, in upper or lower case
bool has_uvm_args()
{
  int argc = sc_core::sc_argc();
  const char* const* argv = sc_core::sc_argv();

  for (int i = 1; argv != nullptr && i < argc; i++)
  {
    const char* a = argv[i];

    if (a != nullptr && (a[0] == '+' || a[0] == '-') &&
        std::toupper(static_cast<unsigned char>(a[1])) == 'U' &&
        std::toupper(static_cast<unsigned char>(a[2])) == 'V' &&
        std::toupper(static_cast<unsigned char>(a[3])) == 'M')
      return true;
  }
  return false;
}

} // anonymous namespace

//----------------------------------------------------------------------
// member function: init (static)
//----------------------------------------------------------------------

void uvm_cmdline_settings::init( uvm_root* top )
{
  int verbosity = UVM_MEDIUM;

  if (settings().initialized)
    return;

  // The arguments are only available once sc_main has been called.
  // Without UVM arguments there is nothing to apply, and the command
  // line processor is not created.
  if (!has_uvm_args())
    return;

  settings().initialized = true;

  if (parse_verbosity(verbosity))
    top->set_report_verbosity_level_hier(verbosity);

  parse_set_verbosity();
  parse_set_action();
  parse_set_severity();

  if (has_arg("+UVM_CONFIG_DB_TRACE"))
    uvm_config_db_options::turn_on_tracing();

  if (has_arg("+UVM_RESOURCE_DB_TRACE"))
    uvm_resource_db_options::turn_on_tracing();
}

//----------------------------------------------------------------------
// member function: apply (static)
//----------------------------------------------------------------------

void uvm_cmdline_settings::apply( uvm_root* top )
{
  init(top);

  if (!settings().initialized)
    return;

  // dump the arguments before they are being used
  do_dump_args(top);

  // issue the reports of the arguments parsed by init
  for (unsigned int i = 0; i < settings().reports.size(); i++)
  {
    const pending_report& r = settings().reports[i];

    if (r.severity == UVM_INFO)
      top->uvm_report_info(r.id, r.message, UVM_NONE);
    else
      top->uvm_report_warning(r.id, r.message, UVM_NONE);
  }
  settings().reports.clear();

  do_time_verbosity_settings();
  do_timeout_settings(top);
  do_factory_settings(top);
  do_config_settings(top);
  do_max_quit_settings(top);
}

//----------------------------------------------------------------------
// member function: get_test_name (static)
//----------------------------------------------------------------------

std::string uvm_cmdline_settings::get_test_name( uvm_root* top,
                                                 const std::string& test_name )
{
  if (!settings().initialized)
    return test_name;

  std::vector<std::string> all_values = get_values("+UVM_TESTNAME=");
  std::vector<std::string> values;

  for (unsigned int i = 0; i < all_values.size(); i++)
  {
    if (!all_values[i].empty())
      values.push_back(all_values[i]);
    else
      top->uvm_report_warning("INVTST",
        "+UVM_TESTNAME provided on the command line without a test name, it will be ignored.",
        UVM_NONE);
  }

  if (values.empty())
    return test_name;

  if (values.size() > 1)
  {
    std::ostringstream str;
    str << "Multiple (" << values.size()
        << ") +UVM_TESTNAME arguments provided on the command line.  '"
        << values[0] << "' will be used.  Provided list: "
        << join(values, ", ") << ".";
    top->uvm_report_warning("MULTTST", str.str(), UVM_NONE);
  }

  return values[0];
}

//----------------------------------------------------------------------
// member function: has_arg (static)
//----------------------------------------------------------------------

bool uvm_cmdline_settings::has_arg( const std::string& match )
{
  std::vector<std::string> args;

  if (!settings().initialized)
    return false;

  return (uvm_cmdline_processor::get_inst()->get_arg_matches(match, args) > 0);
}

//----------------------------------------------------------------------
// member function: apply_report_settings (static)
//----------------------------------------------------------------------

void uvm_cmdline_settings::apply_report_settings( uvm_component* comp )
{
  const settings_t& s = settings();

  if (s.verbosity.empty() && s.action.empty() && s.severity.empty())
    return;

  const std::string name = comp->get_full_name();

  for (unsigned int i = 0; i < s.verbosity.size(); i++)
  {
    const verbosity_setting& setting = s.verbosity[i];

    if (!is_match(setting.comp, name))
      continue;

    if (setting.phase.empty() || setting.phase == "build" ||
        (setting.phase == "time" && setting.offset == sc_core::SC_ZERO_TIME))
      set_verbosity(comp, setting);
  }

  for (unsigned int i = 0; i < s.action.size(); i++)
  {
    const action_setting& setting = s.action[i];

    if (!is_match(setting.comp, name))
      continue;

    if (setting.id == "_ALL_")
    {
      if (setting.all_sev)
        for (unsigned int j = 0; j < 4; j++)
          comp->set_report_severity_action(all_severities[j], setting.action);
      else
        comp->set_report_severity_action(setting.sev, setting.action);
    }
    else
    {
      if (setting.all_sev)
        comp->set_report_id_action(setting.id, setting.action);
      else
        comp->set_report_severity_id_action(setting.sev, setting.id, setting.action);
    }
  }

  for (unsigned int i = 0; i < s.severity.size(); i++)
  {
    const severity_setting& setting = s.severity[i];

    if (!is_match(setting.comp, name))
      continue;

    if (setting.id == "_ALL_")
    {
      if (setting.all_sev)
        for (unsigned int j = 0; j < 4; j++)
          comp->set_report_severity_override(all_severities[j], setting.sev);
      else
        comp->set_report_severity_override(setting.orig_sev, setting.sev);
    }
    else
    {
      if (setting.all_sev)
        for (unsigned int j = 0; j < 4; j++)
          comp->set_report_severity_id_override(all_severities[j], setting.id, setting.sev);
      else
        comp->set_report_severity_id_override(setting.orig_sev, setting.id, setting.sev);
    }
  }
}

//----------------------------------------------------------------------
// member function: apply_verbosity_settings (static)
//----------------------------------------------------------------------

void uvm_cmdline_settings::apply_verbosity_settings( uvm_component* comp,
                                                     const std::string& phase_name )
{
  const std::vector<verbosity_setting>& s = settings().verbosity;

  for (unsigned int i = 0; i < s.size(); i++)
  {
    // settings for the build phase are applied when the component is constructed
    if (s[i].phase != phase_name || s[i].phase == "build" || s[i].phase == "time")
      continue;

    if (is_match(s[i].comp, comp->get_full_name()))
      set_verbosity(comp, s[i]);
  }
}

} // namespace uvm
