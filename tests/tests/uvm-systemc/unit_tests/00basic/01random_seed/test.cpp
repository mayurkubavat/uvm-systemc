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

// Test of the random seed functions uvm_set_random_seed,
// uvm_get_random_seed and uvm_create_random_seed.

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

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name)
  {}

  virtual void report_phase( uvm_phase& phase )
  {
    bool pass = true;

    uvm_set_random_seed(100);
    CHECK(uvm_get_random_seed() == 100);

    // different names give different seeds
    unsigned int a1 = uvm_create_random_seed("type_a", "top.a");
    unsigned int b1 = uvm_create_random_seed("type_a", "top.b");
    unsigned int c1 = uvm_create_random_seed("type_c", "top.a");
    CHECK(a1 != b1);
    CHECK(a1 != c1);

    // a second call with the same names gives a new seed
    unsigned int a2 = uvm_create_random_seed("type_a", "top.a");
    CHECK(a2 != a1);

    // the same global seed gives the same seeds again, also when the
    // seeds are created in another order
    uvm_set_random_seed(100);
    CHECK(uvm_create_random_seed("type_c", "top.a") == c1);
    CHECK(uvm_create_random_seed("type_a", "top.a") == a1);
    CHECK(uvm_create_random_seed("type_a", "top.a") == a2);
    CHECK(uvm_create_random_seed("type_a", "top.b") == b1);

    // another global seed gives other seeds
    uvm_set_random_seed(101);
    CHECK(uvm_create_random_seed("type_a", "top.a") != a1);

    // the instance name is optional
    unsigned int g1 = uvm_create_random_seed("type_a");
    CHECK(uvm_create_random_seed("type_a") != g1);

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
