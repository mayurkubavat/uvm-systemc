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

#ifndef UVM_TLM_TIME_H_
#define UVM_TLM_TIME_H_

#include <string>
#include <systemc>

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_tlm_time
//
//! Canonical time representation for IEEE 1800.2-2020 section 12.3.3
//! TLM-2 sockets, interoperable with sc_core::sc_time.
//----------------------------------------------------------------------

class uvm_tlm_time
{
 public:
  explicit uvm_tlm_time( const std::string& name = "uvm_tlm_time",
                         double res = get_time_resolution() );

  static void set_time_resolution( double res );
  static double get_time_resolution();

  std::string get_name() const;
  void reset();

  void incr( double t, double scaled = 1.0, double secs = 1.0 );
  void incr( const sc_core::sc_time& t );

  void decr( double t, double scaled = 1.0, double secs = 1.0 );
  void decr( const sc_core::sc_time& t );

  double get_realtime( double scaled = 1.0, double secs = 1.0 ) const;
  double get_abstime( double scaled = 1.0 ) const;
  void set_abstime( double t, double scaled = 1.0 );

  sc_core::sc_time to_sc_time() const;
  void from_sc_time( const sc_core::sc_time& t );

  double get_resolution() const;
  std::string to_string() const;

  bool operator==( const uvm_tlm_time& rhs ) const;
  bool operator!=( const uvm_tlm_time& rhs ) const;

 private:
  static double& m_default_res();

  std::string m_name;
  double m_res;
  double m_time;
};

} // namespace uvm

#endif // UVM_TLM_TIME_H_
