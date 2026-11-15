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

// Test of +UVM_TESTNAME: the test given on the command line is run
// instead of the test passed to run_test, and the first one is used when
// the argument is given more than once.

#include <systemc>
#include <uvm>

#include <iostream>

using namespace uvm;

class test_a : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test_a);

  test_a( uvm_component_name name ) : uvm_test(name) {}

  virtual void report_phase( uvm_phase& phase )
  {
    UVM_ERROR("TEST", "test_a was run, expected test_b");
  }
};

class test_b : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test_b);

  test_b( uvm_component_name name ) : uvm_test(name) {}

  virtual void report_phase( uvm_phase& phase )
  {
    std::cout << "test_b run as " << get_full_name() << std::endl;
    std::cout << "** UVM TEST PASSED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test_a");
  return 0;
}

int main(int, char*[])
{
  const char* argv[] = {
    "test",
    "+UVM_TESTNAME=",
    "+UVM_TESTNAME=test_b",
    "+UVM_TESTNAME=test_a"
  };
  int argc = sizeof(argv) / sizeof(argv[0]);

  return sc_core::sc_elab_and_sim(argc, const_cast<char**>(argv));
}
