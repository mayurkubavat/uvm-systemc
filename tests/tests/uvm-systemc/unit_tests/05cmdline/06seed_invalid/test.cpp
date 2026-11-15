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

// Test of the command line argument +UVM_SEED with a value which is
// not a seed. The argument is reported and the default seed is kept.

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

    if (uvm_get_random_seed() == 1)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "** UVM TEST FAILED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");

  return 0;
}

int main(int, char*[])
{
  const char* argv[] = {
    "test",
    "+UVM_SEED=-5"
  };
  int argc = sizeof(argv) / sizeof(argv[0]);

  return sc_core::sc_elab_and_sim(argc, const_cast<char**>(argv));
}
