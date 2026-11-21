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

// Regression test for uvm_mem_region::convert2string(). The image of a
// region shows its offset range in the UVM-SV format ['h%h:'h%h] on
// 64-bit offsets, and is used by uvm_mem_mam::convert2string() and in
// the error message for an overlapping reservation.

#include <systemc>
#include <uvm>

#include <iostream>

using namespace uvm;

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    uvm_mem_mam_cfg* cfg = new uvm_mem_mam_cfg();
    cfg->n_bytes = 1;
    cfg->start_offset = 0;
    cfg->end_offset = 0xfff;
    cfg->mode = uvm_mem_mam::GREEDY;
    cfg->locality = uvm_mem_mam::BROAD;

    uvm_mem_mam* mam = new uvm_mem_mam("mam", cfg);

    uvm_mem_region* r1 = mam->reserve_region(0x10, 0x10);
    uvm_mem_region* r2 = mam->reserve_region(0x200, 0x100);

    if (r1 == nullptr || r2 == nullptr)
      UVM_ERROR("MAMSTR", "regions could not be reserved");

    std::string img = mam->convert2string();
    std::cout << img;

    if (img != "Allocated memory regions:\n"
               "   ['h0000000000000010:'h000000000000001f]\n"
               "   ['h0000000000000200:'h00000000000002ff]\n")
      UVM_ERROR("MAMSTR", "unexpected image of the allocated regions");

    // an overlapping reservation names the region it overlaps with
    uvm_mem_region* r3 = mam->reserve_region(0x18, 4);
    if (r3 != nullptr)
      UVM_ERROR("MAMSTR", "overlapping region was reserved");

    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    uvm_coreservice_t* cs = uvm_coreservice_t::get();
    uvm_report_server* svr = cs->get_report_server();

    // exactly one error is expected: the overlapping reservation
    if (svr->get_severity_count(UVM_FATAL) == 0 &&
        svr->get_severity_count(UVM_ERROR) == 1 &&
        svr->get_id_count("MAMSTR") == 0)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "!! UVM TEST FAILED !!" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
