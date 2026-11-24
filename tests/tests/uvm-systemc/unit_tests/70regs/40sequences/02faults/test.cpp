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

// Test that each built-in register and memory test sequence reports an
// error for a DUT fault it is meant to find, and no error without the
// fault.

#include "../reg_seq_tb.h"

class test : public tb_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : tb_test(name), failed(false) {}

  bool failed;

  template <typename SEQ>
  void check( const std::string& what, bool dut_model::*fault )
  {
    int n;

    dut().clear_faults();
    std::cout << "--- " << what << ": no fault" << std::endl;
    n = run_seq<SEQ>();
    std::cout << "errors: " << n << std::endl;
    if (n != 0) failed = true;

    dut().*fault = true;
    std::cout << "--- " << what << ": with fault" << std::endl;
    n = run_seq<SEQ>();
    std::cout << "errors: " << n << std::endl;
    if (n == 0) failed = true;

    dut().clear_faults();
  }

  void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    uvm_resource_db<bool>::set("REG::" + model->r_ro->get_full_name(),
      "NO_REG_ACCESS_TEST", true, this);

    check<tb_hw_reset_seq>("hardware reset", &dut_model::bad_reset);
    check<tb_reg_access_seq>("register access", &dut_model::stuck_bit);
    check<tb_mem_walk_seq>("memory walk", &dut_model::mem_stuck_bit);
    check<tb_mem_access_seq>("memory access", &dut_model::mem_drop_bit);
    check<tb_shared_access_seq>("shared access", &dut_model::b_write_dead);

    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    if (!failed)
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
