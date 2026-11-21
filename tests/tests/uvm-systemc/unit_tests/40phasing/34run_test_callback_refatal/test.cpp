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

// Test of uvm_root::die with fatal errors reported during the abort: a
// fatal error in a component pre_abort and in a run_test callback
// pre_abort does not abort again; every pre_abort is called once and the
// report summary is printed once.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class my_run_test_cb : public uvm_run_test_callback
{
 public:
  my_run_test_cb( const std::string& name ) : uvm_run_test_callback(name) {}

  virtual void pre_run_test()
  {
    std::cout << sc_core::sc_time_stamp() << ": " << get_name() << " pre_run_test" << std::endl;
  }

  virtual void post_run_test()
  {
    std::cout << sc_core::sc_time_stamp() << ": " << get_name() << " post_run_test" << std::endl;
  }

  virtual void pre_abort()
  {
    std::cout << sc_core::sc_time_stamp() << ": " << get_name() << " pre_abort" << std::endl;
    UVM_FATAL("CB", "fatal error in pre_abort of " + get_name());
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    sc_core::wait(10, sc_core::SC_NS);
    UVM_FATAL("TEST", "fatal error terminates the test");
    phase.drop_objection(this);
  }

  virtual void pre_abort()
  {
    std::cout << sc_core::sc_time_stamp() << ": test pre_abort" << std::endl;
    UVM_FATAL("TEST", "fatal error in pre_abort of the test");
  }
};

int sc_main(int, char*[])
{
  my_run_test_cb cb1("cb1"), cb2("cb2");
  uvm_run_test_callback::add(&cb1);
  uvm_run_test_callback::add(&cb2);

  run_test("test");
  return 0;
}
