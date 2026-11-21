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

// An address map cannot be cloned. Like the other register model
// classes, uvm_reg_map::clone() reports a fatal error instead of
// silently returning a null pointer. The fatal error is demoted by a
// report catcher so the test can check that it was issued.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class blk : public uvm_reg_block
{
 public:
  blk( const std::string& name = "blk" ) : uvm_reg_block(name)
  {}

  virtual void build()
  {
    default_map = create_map("map", 0, 4, UVM_LITTLE_ENDIAN);
    lock_model();
  }
};

class clone_catcher : public uvm_report_catcher
{
 public:
  static int seen;

  virtual action_e do_catch()
  {
    if (get_severity() == UVM_FATAL &&
        get_id() == "UVM/REGMAP/NOCLONE")
    {
      std::cout << "Fatal message caught: " << get_message() << std::endl;
      seen++;
      set_severity(UVM_WARNING);
      set_action(UVM_DISPLAY);
    }
    return THROW;
  }
};

int clone_catcher::seen = 0;

int sc_main(int, char*[])
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_report_server* svr = cs->get_report_server();

  blk* b = new blk();
  b->build();

  clone_catcher* c = new clone_catcher();
  uvm_report_cb::add(nullptr, c);

  uvm::uvm_object* obj = b->default_map->clone();

  if (obj != nullptr)
    UVM_ERROR("Test", "uvm_reg_map::clone() returned an object");

  if (clone_catcher::seen != 1)
    UVM_ERROR("Test", "Fatal message about uvm_reg_map::clone() not seen");

  svr->report_summarize();

  if (svr->get_severity_count(UVM_FATAL) +
      svr->get_severity_count(UVM_ERROR) == 0)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "!! UVM TEST FAILED !!" << std::endl;

  return 0;
}
