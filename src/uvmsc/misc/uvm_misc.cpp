//----------------------------------------------------------------------
//   Copyright 2019 COSEDA Technologies GmbH
//   Copyright 2012-2021 NXP B.V.
//   Copyright 2014 Fraunhofer-Gesellschaft zur Foerderung
//					der angewandten Forschung e.V.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2011 Cadence Design Systems, Inc.
//   Copyright 2010 Synopsys, Inc.
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

#include <iostream>
#include <bitset>
#include <cctype>
#include <climits>
#include <algorithm>
#include <iostream>
#include <string>
#include <functional>
#include <iomanip>
#include <sstream>
#include <cstdarg>
#include <map>
#include <utility>

#include <systemc>

#include "uvmsc/base/uvm_object_globals.h"
#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_root.h"
#include "uvmsc/base/uvm_coreservice_t.h"
#include "uvmsc/base/uvm_default_coreservice_t.h"

using namespace sc_core;

namespace uvm {

//----------------------------------------------------------------------
// Local functions for the UVM_STRING and UVM_TIME radixes
//----------------------------------------------------------------------

// Format "%0s": each group of 8 bits, starting from the most significant
// one, is a character; leading null characters are not printed
static std::string m_uvm_bits_to_ascii( const sc_dt::sc_unsigned& value, int size )
{
  std::string str;
  for( int i = (size + 7) / 8 - 1; i >= 0; i-- )
  {
    int hi = std::min(8 * i + 7, size - 1);
    char c = static_cast<char>(value.range(hi, 8 * i).to_uint());
    if (c == 0 && str.empty())
      continue;
    str += c;
  }
  return str;
}

// Formats "%0b", "%0o" and "%0x": the digits without leading zeros
static std::string m_uvm_strip_leading_zeros( const std::string& digits )
{
  std::string::size_type first = digits.find_first_not_of('0');
  if (first == std::string::npos)
    return "0";
  return digits.substr(first);
}

// Format "%0t": the value is a number of time resolution units.
// Building or printing an sc_time fixes the time resolution, which may
// still be set while the simulation is not running; print the plain
// number of time resolution units then, as %0t does by default.
static std::string m_uvm_value_to_time( sc_dt::uint64 value )
{
  if (!sc_core::sc_is_running())
  {
    std::ostringstream str;
    str << value;
    return str.str();
  }
  return sc_core::sc_time::from_value(value).to_string();
}

//----------------------------------------------------------------------
// Global function: uvm_vector_to_string
//
//! Implementation defined
//! Convert integral value in a string using formatting based on radix
//----------------------------------------------------------------------

std::string uvm_vector_to_string(const uvm_integral_t& val,
                             int size,
                             uvm_radix_enum radix,
                             const std::string& radix_str )
{
  sc_dt::sc_uint_base reduced_value (val, size);
  long value = reduced_value;
  std::ostringstream rstr;
  switch(radix) {
    case UVM_BIN:      // format "%0s%0b"
    {
      rstr << radix_str
           << m_uvm_strip_leading_zeros(std::bitset<64>(reduced_value.to_uint64()).to_string());
      return rstr.str();
    }
    case UVM_OCT:      // format "%0s%0o"
    {
      rstr << radix_str << std::oct << value;
      return rstr.str();
    }
    case UVM_UNSIGNED: // format "%0s%0d"
    {
      rstr << radix_str << std::dec << reduced_value.to_uint64();
      return rstr.str();
    }
    case UVM_STRING:  // format "%0s%0s"
    {
      sc_dt::sc_unsigned uvalue(size);
      uvalue = reduced_value.to_uint64();
      rstr << radix_str << m_uvm_bits_to_ascii(uvalue, size);
      return rstr.str();
    }
    case UVM_TIME:    // format "%0s%0t"
    {
      rstr << radix_str << m_uvm_value_to_time(reduced_value.to_uint64());
      return rstr.str();
    }
    case UVM_DEC:    // format "%0s%0d"
    {
      rstr << radix_str << std::dec << value;
      return rstr.str();
    }
    default:         //  format "%0s%0x"
    {
      rstr << radix_str << std::hex << value;
      return rstr.str();
    }
  }
}

//----------------------------------------------------------------------
// Global function: uvm_vector_to_string
//
//! Implementation defined
//! Convert value of uvm_bitstream_t type in a string using
//! formatting based on radix
//----------------------------------------------------------------------

std::string uvm_vector_to_string( const uvm_bitstream_t& val,
                             	  int size,
                             	  uvm_radix_enum radix,
                             	  const std::string& radix_str )
{
  // Copy over the contents over to the underlying data structure
  sc_dt::sc_unsigned value (size);
  value = val;
  std::ostringstream rstr;

  switch(radix) {
    case UVM_BIN:      // format "%0s%0b"
    {
      rstr << radix_str << m_uvm_strip_leading_zeros(value.to_string(sc_dt::SC_BIN_US, false));
      return rstr.str();
    }
    case UVM_OCT:      // format "%0s%0o"
    {
      rstr << radix_str << m_uvm_strip_leading_zeros(value.to_string(sc_dt::SC_OCT_US, false));
      return rstr.str();
    }
    case UVM_UNSIGNED: // format "%0s%0d"
    {
      rstr << radix_str << value.to_string(sc_dt::SC_DEC, false);
      return rstr.str();
    }
    case UVM_STRING:  // format "%0s%0s"
    {
      rstr << radix_str << m_uvm_bits_to_ascii(value, size);
      return rstr.str();
    }
    case UVM_TIME:    // format "%0s%0t"
    {
      // saturate a value that does not fit in sc_time
      sc_dt::uint64 t = ~sc_dt::UINT64_ZERO;
      if (size <= 64 || (value >> 64) == 0)
        t = value.to_uint64();
      rstr << radix_str << m_uvm_value_to_time(t);
      return rstr.str();
    }
    case UVM_DEC:    // format "%0s%0d"
    {
      rstr << radix_str << value.to_string(sc_dt::SC_DEC, false);
      return rstr.str();
    }
    default:         //  format "%0s%0x"
    {
      rstr << radix_str << m_uvm_strip_leading_zeros(value.to_string(sc_dt::SC_HEX_US, false));
      return rstr.str();
    }
  }
}
const char* uvm_apprepend_name[] = {
 "UVM_APPEND",
 "UVM_PREPEND"
};

//----------------------------------------------------------------------
// Global function: uvm_flatten_name
//
// Implementation defined
//----------------------------------------------------------------------
std::string uvm_flatten_name( const std::string& full_name, const char* scope_separator )
{
  std::string s = full_name;
  replace( s.begin(), s.end(), '.', *scope_separator);
  return s;
}


//----------------------------------------------------------------------
// Random seeding
//
// The global seed and the table of the seeds which were created from it.
// Both are function-local statics so that they can be used during static
// initialization.
//----------------------------------------------------------------------

namespace {

unsigned int& m_global_random_seed()
{
  static unsigned int seed = 1;
  return seed;
}

typedef std::map<std::pair<std::string, std::string>,
                 std::pair<unsigned int, unsigned int> > uvm_seed_table_t;

uvm_seed_table_t& m_random_seed_table()
{
  static uvm_seed_table_t table;
  return table;
}

// FNV-1a hash. std::hash is not used, since its result differs between
// standard library implementations.
unsigned int m_oneway_hash( const std::string& str, unsigned int seed )
{
  unsigned int hash = 2166136261u ^ seed;
  for (std::string::size_type i = 0; i < str.size(); i++)
  {
    hash ^= static_cast<unsigned char>(str[i]);
    hash *= 16777619u;
  }
  return hash;
}

} // anonymous namespace

//----------------------------------------------------------------------
// Global function: uvm_set_random_seed
//
//! Implementation defined
//!
//! Sets the global seed from which all seeds returned by
//! uvm_create_random_seed are derived. Seeds which were created before
//! are forgotten. Should be called before run_test.
//----------------------------------------------------------------------

void uvm_set_random_seed( unsigned int seed )
{
  m_global_random_seed() = seed;
  m_random_seed_table().clear();
}

//----------------------------------------------------------------------
// Global function: uvm_get_random_seed
//
//! Implementation defined
//!
//! Returns the global seed (uvm_global_random_seed in UVM-SystemVerilog).
//----------------------------------------------------------------------

unsigned int uvm_get_random_seed()
{
  return m_global_random_seed();
}

//----------------------------------------------------------------------
// Global function: uvm_create_random_seed
//
//! Creates a seed which is unique for the combination of the type name
//! \p type_id and the instance name \p inst_id. The seed only depends on
//! these two names and on the global seed, so it is stable when other
//! parts of the environment change. Repeated calls with the same names
//! return a new seed each time.
//----------------------------------------------------------------------

unsigned int uvm_create_random_seed( const std::string& type_id,
                                     const std::string& inst_id )
{
  const std::string inst = inst_id.empty() ? "__global__" : inst_id;

  uvm_seed_table_t& table = m_random_seed_table();
  std::pair<std::string, std::string> key(inst, type_id);

  uvm_seed_table_t::iterator it = table.find(key);
  if (it == table.end())
  {
    unsigned int seed = m_oneway_hash(type_id + "::" + inst, m_global_random_seed());
    it = table.insert(std::make_pair(key, std::make_pair(seed, 0u))).first;
  }

  unsigned int& seed = it->second.first;
  unsigned int& count = it->second.second;

  seed += count;
  count++;

  return seed;
}

//----------------------------------------------------------------------
// Global function: uvm_leaf_scope
//
// Implementation defined
//----------------------------------------------------------------------

std::string uvm_leaf_scope( const std::string& full_name, const char* scope_separator )
{
  char bracket_match = '\0';
  int  pos = 0;
  int  bmatches = 0;
  std::string ret;

  char ch = *scope_separator;
  switch(ch)
  {
    case '[': bracket_match = ']'; break;
    case '(': bracket_match = ')'; break;
    case '<': bracket_match = '>'; break;
    case '{': bracket_match = '}'; break;
    default : bracket_match = '\0'; break;
  }

  // Only use bracket matching if the input std::string has the end match
  if( (bracket_match != '\0') && (bracket_match != full_name[full_name.length()-1]) )
    bracket_match = '\0';

  for( pos = (int)full_name.length()-1; pos != 0; --pos)
  {
    if( bracket_match == full_name[pos] )
      bmatches++;
    else
      if( *scope_separator == full_name[pos] )
      {
        bmatches--;
        if(!bmatches ||  bracket_match == '\0') break;
      }
  }

  if(pos)
  {
    if( *scope_separator != '.') pos--;
    ret = full_name.substr(pos+1,full_name.length()-1);
  }
  else
    ret = full_name;

  return ret;
}

//----------------------------------------------------------------------
// Global function: uvm_object_value_str
//
//! Return instance id if it exists, otherwise return nullptr
//----------------------------------------------------------------------

std::string uvm_object_value_str( const uvm_object* v )
{
  if (v == nullptr)
    return "<nullptr>";

  std::ostringstream str;
  str << "@" << std::setw(3) << std::setfill('0') << v->get_inst_id();
  return str.str();
}

//----------------------------------------------------------------------
// Global function: uvm_string_queue_join
//
//! Concatenates strings in a vector to a single string
//----------------------------------------------------------------------

const std::string uvm_string_queue_join( const std::vector<std::string>& q )
{
  std::string s;
  s.clear();
  s = "";

  for( unsigned int i = 0; i < q.size(); i++ ) s += q[i];

  return s;
}


//----------------------------------------------------------------------
// Global function: uvm_toupper
//
//! Implementation defined
//! Convert std::string in uppercase
//----------------------------------------------------------------------

std::string uvm_toupper( const std::string& str )
{
  std::string s = str;
  std::transform(s.begin(), s.end(), s.begin(), [](const unsigned char c) { return std::toupper(c); });
  return s;
}

//----------------------------------------------------------------------
// Global function: uvm_realtobits
//
//! Implementation defined
//! Convert double precision value to 64 bits representation
//! need to pack and unpack real values
//----------------------------------------------------------------------

sc_dt::sc_uint<64> uvm_realtobits( double val )
{
  sc_dt::scfx_ieee_double id(val); // convert to IEEE 754 bitfield

  // footprint for 64-bit double precision (double)
  // sign size 1       [63]
  // exponent size 11  [62-52]
  // mantissa1 size 32 [51-20]
  // mantissa0 size 20 [19-0]

  bool               sgn = id.negative();
  sc_dt::sc_uint<11> exp = id.exponent();
  sc_dt::sc_uint<52> mnt = ( sc_dt::uint64( id.mantissa1() ) << 20 ) | id.mantissa0();

  // concatenate parts to bitvector
  sc_dt::sc_uint<64> bits;
  bits = ( sgn, exp, mnt );
  return bits;
}

//----------------------------------------------------------------------
// Global function: uvm_realtobits
//
//! Implementation defined
//! Convert single precision value in 32 bits representation
//! need to pack and unpack real values
//----------------------------------------------------------------------

sc_dt::sc_uint<32> uvm_realtobits( float val )
{
  sc_dt::scfx_ieee_float id(val); // convert to IEEE 754 bitfield

  // footprint for 32-bit single precision (float)
  // sign size 1       [31]
  // exponent size 8   [30-23]
  // mantissa1 size 23 [22-0]

  bool               sgn = id.negative();
  sc_dt::sc_uint<8> exp = id.exponent();
  sc_dt::sc_uint<23> mnt = id.mantissa();

  // concatenate parts to bitvector
  sc_dt::sc_uint<32> bits;
  bits = ( sgn, exp, mnt );
  return bits;
}


//----------------------------------------------------------------------
// Global function: uvm_bitstoreal
//
//! Implementation defined
//! Convert 64 bits representation in a double precision value (double)
//! need to pack and unpack real values
//----------------------------------------------------------------------

double uvm_bitstoreal( const sc_dt::sc_uint<64>& val )
{
  sc_dt::scfx_ieee_double id;

  id.negative(val[63].to_bool());
  id.exponent(val.range(62,52).to_uint());
  id.mantissa0(val.range(19,0).to_uint());
  id.mantissa1(val.range(51,20).to_uint());

  double d = id; // reassign to double
  return d;
}

//----------------------------------------------------------------------
// Global function: uvm_bitstoreal
//
//! Implementation defined
//! Convert 32 bits representation in a single precision value (float)
//! need to pack and unpack real values
//----------------------------------------------------------------------

float uvm_bitstoreal( const sc_dt::sc_uint<32>& val )
{
  sc_dt::scfx_ieee_float id;

  id.negative(val[31].to_bool());
  id.exponent(val.range(30,23).to_uint());
  id.mantissa(val.range(22,0).to_uint());

  float d = id; // reassign to float
  return d;
}

//----------------------------------------------------------------------
// Global function: uvm_sformatf
//
//! Implementation defined
//----------------------------------------------------------------------

std::string uvm_sformatf(const char* format, ...)
{
  char s[500];
  va_list argptr;
  va_start(argptr, format);
  sprintf(s, format, argptr);
  va_end(argptr);
  return std::string(s);
}

//----------------------------------------------------------------------
// Global function: enable_hdl_access
//
//! Implementation defined
//----------------------------------------------------------------------

void enable_hdl_access(sc_core::sc_object* dut)
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_root* top = cs->get_root();

  top->m_hdl_obj = dut;
}


} /* namespace uvm */
