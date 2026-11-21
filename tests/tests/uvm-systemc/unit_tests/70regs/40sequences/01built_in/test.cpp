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

// Test of uvm_reg_mem_built_in_seq: all built-in register and memory
// test sequences run without errors on a DUT without faults, a subset
// can be selected, and registers excluded by a resource are not tested.

#include "../reg_seq_tb.h"

class test : public tb_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : tb_test(name) {}

  void run_phase( uvm_phase& phase )
  {
    int n;

    phase.raise_objection(this);

    // a register with read-only fields only cannot be access tested
    uvm_resource_db<bool>::set("REG::" + model->r_ro->get_full_name(),
      "NO_REG_ACCESS_TEST", true, this);

    std::cout << "--- all built-in sequences" << std::endl;
    n = run_seq<uvm_reg_mem_built_in_seq>();
    std::cout << "errors: " << n << std::endl;

    std::cout << "--- hardware reset and memory walk only" << std::endl;
    n = run_seq<uvm_reg_mem_built_in_seq>(UVM_DO_REG_HW_RESET | UVM_DO_MEM_WALK);
    std::cout << "errors: " << n << std::endl;

    std::cout << "--- register and memory access" << std::endl;
    n = run_seq<uvm_reg_mem_access_seq>();
    std::cout << "errors: " << n << std::endl;

    std::cout << "--- block excluded from all tests" << std::endl;
    uvm_resource_db<bool>::set("REG::" + model->get_full_name(),
      "NO_REG_TESTS", true, this);
    n = run_seq<uvm_reg_mem_built_in_seq>();
    std::cout << "errors: " << n << std::endl;

    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    if (errors() == 0)
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
