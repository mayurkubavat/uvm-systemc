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

// Regression test for UVM_OBJECT_UTILS uvm_report() severity dispatch
// and default verbosity (-1) on both uvm_object and uvm_sequence_item,
// plus uvm::uvm_report_{info,warning,error,fatal} context_name and
// report_enabled_checked forwarding.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class my_obj : public uvm_object
{
 public:
  UVM_OBJECT_UTILS(my_obj);

  my_obj( const std::string& name = "my_obj" ) : uvm_object(name) {}
};

class my_seq_item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(my_seq_item);

  my_seq_item( const std::string& name = "my_seq_item" ) : uvm_sequence_item(name) {}
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    uvm_coreservice_t* cs = uvm_coreservice_t::get();
    uvm_root* top = cs->get_root();
    uvm_report_server* svr = cs->get_report_server();

    top->set_report_verbosity_level_hier(UVM_LOW);
    top->set_report_severity_action(UVM_FATAL, UVM_DISPLAY | UVM_COUNT);

    my_obj obj("obj");
    my_seq_item item("item");

    // 1. uvm_object UVM_OBJECT_UTILS uvm_report severity + default -1 verbosity
    obj.uvm_report(UVM_INFO,    "OBJ_INF", "obj info",    UVM_LOW);
    obj.uvm_report(UVM_WARNING, "OBJ_WRN", "obj warning", UVM_LOW);
    obj.uvm_report(UVM_ERROR,   "OBJ_ERR", "obj error");  // default verbosity -1 -> UVM_LOW
    obj.uvm_report(UVM_FATAL,   "OBJ_FTL", "obj fatal");  // default verbosity -1 -> UVM_NONE

    // 2. uvm_sequence_item UVM_OBJECT_UTILS uvm_report severity + default -1 verbosity
    item.uvm_report(UVM_INFO,    "SEQ_INF", "seq info",    UVM_LOW);
    item.uvm_report(UVM_WARNING, "SEQ_WRN", "seq warning", UVM_LOW);
    item.uvm_report(UVM_ERROR,   "SEQ_ERR", "seq error"); // default verbosity -1 -> UVM_LOW
    item.uvm_report(UVM_FATAL,   "SEQ_FTL", "seq fatal"); // default verbosity -1 -> UVM_NONE

    // 3. uvm_globals context_name + report_enabled_checked forwarding
    uvm::uvm_report_info("GLB_CTX", "glb ctx low", UVM_LOW, "", 0, "ctx_low", false);
    uvm::uvm_report_info("GLB_INF", "glb info high", UVM_HIGH, "", 0, "ctx_info", true);
    uvm::uvm_report_warning("GLB_WRN", "glb warn high", UVM_HIGH, "", 0, "ctx_warn", true);
    uvm::uvm_report_error("GLB_ERR", "glb err high", UVM_HIGH, "", 0, "ctx_err", true);
    uvm::uvm_report_fatal("GLB_FTL", "glb fatal high", UVM_HIGH, "", 0, "ctx_ftl", true);

    int warn_cnt = svr->get_severity_count(UVM_WARNING);
    int err_cnt  = svr->get_severity_count(UVM_ERROR);
    int ftl_cnt  = svr->get_severity_count(UVM_FATAL);

    std::cout << "counts: warn=" << warn_cnt
              << " err=" << err_cnt
              << " fatal=" << ftl_cnt << std::endl;

    if (warn_cnt == 3 && err_cnt == 3 && ftl_cnt == 3)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "!! UVM TEST FAILED !!" << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
