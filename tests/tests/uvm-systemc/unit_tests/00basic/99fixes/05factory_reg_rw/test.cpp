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

// The library does not register types of its own under names which
// are common in user code. A user class reg_rw, as in the register
// examples, is created by the factory under its name.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

class reg_rw : public uvm::uvm_sequence_item
{
 public:
  int marker;

  reg_rw( const std::string& name = "reg_rw" ) : uvm::uvm_sequence_item(name), marker(42)
  {}

  UVM_OBJECT_UTILS(reg_rw);
};

int sc_main(int, char*[])
{
  uvm::uvm_coreservice_t* cs = uvm::uvm_coreservice_t::get();
  uvm::uvm_factory* factory = cs->get_factory();
  uvm::uvm_report_server* svr = cs->get_report_server();

  uvm::uvm_object* obj = factory->create_object_by_name("reg_rw", "", "obj");
  reg_rw* rw = dynamic_cast<reg_rw*>(obj);

  if (rw == nullptr)
  {
    UVM_ERROR("Test", "The factory did not create the user class reg_rw");
  }
  else
    std::cout << "created the user class reg_rw, marker " << rw->marker << std::endl;

  svr->report_summarize();

  if (svr->get_severity_count(uvm::UVM_WARNING) + svr->get_severity_count(uvm::UVM_ERROR) +
      svr->get_severity_count(uvm::UVM_FATAL) == 0)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "!! UVM TEST FAILED !!" << std::endl;

  return 0;
}
