//----------------------------------------------------------------------
//   Copyright 2012-2014 NXP B.V.
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

#ifndef UVM_PROCESS_H_
#define UVM_PROCESS_H_

#include <random>
#include <string>

#include <systemc>

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_process
//
//! IEEE 1800.2-2020 process wrapper backed by sc_core::sc_process_handle.
//----------------------------------------------------------------------

class uvm_process
{
 public:
  enum state { FINISHED, RUNNING, WAITING, SUSPENDED, KILLED };

  static uvm_process* self();
  static uvm_process* from_handle( const sc_core::sc_process_handle& handle );

  state status() const;

  void kill();
  void await();
  void suspend();
  void resume();

  void srandom( unsigned int seed );
  std::string get_randstate() const;
  void set_randstate( const std::string& state_str );

  sc_core::sc_process_handle get_process_handle() const;
  std::mt19937& get_rng();

  ~uvm_process() = default;

 private:
  explicit uvm_process( const sc_core::sc_process_handle& handle );
  uvm_process( const uvm_process& ) = delete;
  uvm_process& operator=( const uvm_process& ) = delete;

  sc_core::sc_process_handle m_handle;
  bool m_killed;
  std::mt19937 m_rng;
};

} // namespace uvm

#endif /* UVM_PROCESS_H_ */
