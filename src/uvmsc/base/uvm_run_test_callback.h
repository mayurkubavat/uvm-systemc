//----------------------------------------------------------------------
//   Copyright 2018 Cadence Design Systems, Inc.
//   Copyright 2018-2024 NVIDIA Corporation
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

#ifndef UVM_RUN_TEST_CALLBACK_H_
#define UVM_RUN_TEST_CALLBACK_H_

#include <string>
#include <vector>

#include "uvmsc/cb/uvm_callback.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_run_test_callback
//
//! Callback class, which allows an application to execute code before
//! and after uvm_root::run_test, and before the simulation is aborted
//! by uvm_root::die. Callbacks are registered with the static member
//! function add.
//----------------------------------------------------------------------

class uvm_run_test_callback : public uvm_callback
{
 public:
  //! The constructor shall create a new callback with the given name.
  explicit uvm_run_test_callback( const std::string& name = "uvm_run_test_callback" );

  //! This callback is called at the start of uvm_root::run_test, before
  //! the test is created and before any phase is executed.
  virtual void pre_run_test() {}

  //! This callback is called after all phases have completed, before
  //! the report summary is printed by uvm_root::run_test.
  virtual void post_run_test() {}

  //! This callback is called by uvm_root::die, after the member function
  //! pre_abort of all components has been called, and before the report
  //! summary is printed.
  virtual void pre_abort() {}

  //! Registers the callback cb. Returns true if the callback was added,
  //! false if cb is a null pointer or was already registered.
  static bool add( uvm_run_test_callback* cb );

  //! Removes the registered callback cb. Returns true if the callback was
  //! removed, false if cb is a null pointer or was not registered. The SV
  //! name delete is a reserved word in C++.
  static bool do_delete( uvm_run_test_callback* cb );

  virtual const std::string get_type_name() const;

  virtual ~uvm_run_test_callback();

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  // execute pre_run_test, post_run_test or pre_abort of all
  // registered callbacks, in the order of registration
  static void m_do_pre_run_test();
  static void m_do_post_run_test();
  static void m_do_pre_abort();

 private:
  static std::vector<uvm_run_test_callback*>& m_registered_cbs();
}; // class uvm_run_test_callback

} // namespace uvm

#endif // UVM_RUN_TEST_CALLBACK_H_
