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

// Test of uvm_run_test_callback::do_delete called by a callback: a
// callback deleted by an earlier one is not called any more, also when it
// is destroyed after it was deleted. A callback added by a callback is
// called from the next run_test callback on.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class my_run_test_cb : public uvm_run_test_callback
{
 public:
  my_run_test_cb* to_delete;
  my_run_test_cb* to_destroy;
  my_run_test_cb* to_add;

  my_run_test_cb( const std::string& name )
  : uvm_run_test_callback(name), to_delete(nullptr), to_destroy(nullptr),
    to_add(nullptr) {}

  virtual void pre_run_test()
  {
    std::cout << sc_core::sc_time_stamp() << ": " << get_name() << " pre_run_test" << std::endl;
    if (to_delete != nullptr)
      std::cout << "delete " << to_delete->get_name() << " returns "
                << uvm_run_test_callback::do_delete(to_delete) << std::endl;
    if (to_destroy != nullptr)
    {
      std::cout << "delete " << to_destroy->get_name() << " returns "
                << uvm_run_test_callback::do_delete(to_destroy) << std::endl;
      delete to_destroy;
    }
    if (to_add != nullptr)
      std::cout << "add " << to_add->get_name() << " returns "
                << uvm_run_test_callback::add(to_add) << std::endl;
    to_delete = to_destroy = to_add = nullptr;
  }

  virtual void post_run_test()
  {
    std::cout << sc_core::sc_time_stamp() << ": " << get_name() << " post_run_test" << std::endl;
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}
};

int sc_main(int, char*[])
{
  my_run_test_cb cb1("cb1"), cb2("cb2"), cb4("cb4"), cb5("cb5");
  my_run_test_cb* cb3 = new my_run_test_cb("cb3");
  cb1.to_delete = &cb2;
  cb1.to_destroy = cb3;
  cb1.to_add = &cb5;
  uvm_run_test_callback::add(&cb1);
  uvm_run_test_callback::add(&cb2);
  uvm_run_test_callback::add(cb3);
  uvm_run_test_callback::add(&cb4);

  run_test("test");
  return 0;
}
