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

// uvm_hdl_read and uvm_hdl_deposit report clearly when the object in
// the design exists but has another value type than the access.

#include <systemc>
#include <uvm>

#include <iostream>

#include "reg.h"

using namespace uvm;

class dut : public sc_core::sc_module
{
 public:
  reg<sc_dt::sc_uint<16> > narrow;
  sc_core::sc_signal<sc_dt::sc_uint<16>, sc_core::SC_MANY_WRITERS> sig;

  dut( sc_core::sc_module_name name )
  : sc_core::sc_module(name), narrow("narrow"), sig("sig")
  {
    narrow = 0x1234;
  }
};

int sc_main(int, char*[])
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_report_server* svr = cs->get_report_server();

  dut top("top");

  // the value type of the object
  sc_dt::sc_uint<16> right = 0;
  bool ok = uvm_hdl_read("top.narrow", right);
  std::cout << "read with sc_uint<16>: " << (ok ? "ok" : "failed")
            << ", value 0x" << std::hex << right << std::dec << std::endl;

  // the type of register backdoor accesses
  std::cout << "four type mismatches are expected" << std::endl;
  uvm_reg_data_t wrong = 0;
  ok = uvm_hdl_read("top.narrow", wrong);
  std::cout << "read with uvm_reg_data_t: " << (ok ? "ok" : "failed") << std::endl;
  ok = uvm_hdl_deposit("top.narrow", wrong);
  std::cout << "deposit with uvm_reg_data_t: " << (ok ? "ok" : "failed") << std::endl;
  ok = uvm_hdl_read("top.sig", wrong);
  std::cout << "read signal with uvm_reg_data_t: " << (ok ? "ok" : "failed") << std::endl;
  ok = uvm_hdl_deposit("top.sig", wrong);
  std::cout << "deposit signal with uvm_reg_data_t: " << (ok ? "ok" : "failed") << std::endl;

  svr->report_summarize();

  if (svr->get_severity_count(UVM_ERROR) == 4 && svr->get_severity_count(UVM_FATAL) == 0)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "!! UVM TEST FAILED !!" << std::endl;

  return 0;
}
