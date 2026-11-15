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

// Test of the command line argument +UVM_SEED, which sets the global
// seed. The first of several arguments is used. The seed is also set
// for a component which is constructed before run_test is called.

#include <systemc>
#include <uvm>

#include <iostream>

using namespace uvm;

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // the seed is set before the test is built
    std::cout << "global seed = " << uvm_get_random_seed() << std::endl;

    if (uvm_get_random_seed() == 4711)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "** UVM TEST FAILED **" << std::endl;
  }
};

class early : public uvm_component
{
 public:
  early( uvm_component_name name ) : uvm_component(name)
  {
    std::cout << "global seed in sc_main = " << uvm_get_random_seed() << std::endl;
  }
};

int sc_main(int, char*[])
{
  early e("early");

  run_test("test");

  return 0;
}

int main(int, char*[])
{
  const char* argv[] = {
    "test",
    "+UVM_SEED=4711",
    "+UVM_SEED=12"
  };
  int argc = sizeof(argv) / sizeof(argv[0]);

  return sc_core::sc_elab_and_sim(argc, const_cast<char**>(argv));
}
