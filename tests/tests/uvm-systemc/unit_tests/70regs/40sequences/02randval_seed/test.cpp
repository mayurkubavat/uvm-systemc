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

// Test of the seeding of uvm_reg_randval, which generates the data
// values of the built-in register and memory test sequences. The
// values depend on the global seed only.

#include <systemc>
#include <uvm>

using namespace uvm;

#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::cout << "FAILED: " << #cond << std::endl; \
      pass = false; \
    } \
  } while (false)

// the first value of a new generator
uvm_reg_data_t first_value()
{
  uvm_reg_randval r;
  r.randomize();
  return r.randval;
}

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name)
  {}

  virtual void report_phase( uvm_phase& phase )
  {
    bool pass = true;

    // the same global seed gives the same values
    uvm_set_random_seed(100);
    uvm_reg_data_t a1 = first_value();
    uvm_reg_data_t a2 = first_value();
    uvm_set_random_seed(100);
    CHECK(first_value() == a1);
    CHECK(first_value() == a2);

    // two generators do not give the same values
    CHECK(a1 != a2);

    // another global seed gives other values
    uvm_set_random_seed(101);
    CHECK(first_value() != a1);

    // a seed which is set explicitly is not changed by the global seed
    uvm_reg_randval r1, r2;
    r1.set_seed(5);
    uvm_set_random_seed(102);
    r2.set_seed(5);
    r1.randomize();
    r2.randomize();
    CHECK(r1.randval == r2.randval);

    // has_seed tells whether a seed has been set or used
    uvm_reg_randval r3;
    CHECK(!r3.has_seed());
    r3.randomize();
    CHECK(r3.has_seed());
    CHECK(r1.has_seed());

    if (pass)
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
