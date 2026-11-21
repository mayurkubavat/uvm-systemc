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

// Test of uvm_run_test_callback: the callbacks pre_run_test and
// post_run_test of all registered callbacks are called by run_test, in
// the order of registration; add and do_delete report whether the list
// of callbacks changed.

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
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name)
  {
    std::cout << sc_core::sc_time_stamp() << ": test constructed" << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    sc_core::wait(10, sc_core::SC_NS);
    std::cout << sc_core::sc_time_stamp() << ": test run_phase done" << std::endl;
    phase.drop_objection(this);
  }
};

static void show( const std::string& what, bool result )
{
  std::cout << what << ": " << (result ? "true" : "false") << std::endl;
}

int sc_main(int, char*[])
{
  my_run_test_cb cb1("cb1"), cb2("cb2"), cb3("cb3");

  show("add cb1", uvm_run_test_callback::add(&cb1));
  show("add cb2", uvm_run_test_callback::add(&cb2));
  show("add cb3", uvm_run_test_callback::add(&cb3));
  show("add cb1 again", uvm_run_test_callback::add(&cb1));
  show("add nullptr", uvm_run_test_callback::add(nullptr));
  show("delete cb2", uvm_run_test_callback::do_delete(&cb2));
  show("delete cb2 again", uvm_run_test_callback::do_delete(&cb2));
  show("delete nullptr", uvm_run_test_callback::do_delete(nullptr));

  {
    // a callback which is destroyed removes itself
    my_run_test_cb tmp("tmp");
    show("add tmp", uvm_run_test_callback::add(&tmp));
  }

  run_test("test");
  return 0;
}
