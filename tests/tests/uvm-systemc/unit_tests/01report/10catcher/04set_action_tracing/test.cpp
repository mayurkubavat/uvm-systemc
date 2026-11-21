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

// Regression test for uvm_report_catcher::set_action() setting
// m_set_action_called across severity promotions and preserving
// uvm_callbacks_base::m_tracing == false across report emission.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class promo_catcher : public uvm_report_catcher
{
 public:
  virtual action_e do_catch()
  {
    if (get_id() == "PROMOTE_FATAL")
    {
      set_severity(UVM_FATAL);
      set_action(UVM_DISPLAY | UVM_COUNT);
      return THROW;
    }
    if (get_id() == "PROMOTE_ERR")
    {
      set_severity(UVM_ERROR);
      set_action(UVM_DISPLAY);
      return THROW;
    }
    return THROW;
  }
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
    uvm_report_server* svr = cs->get_report_server();
    svr->set_max_quit_count(5);

    promo_catcher ctchr;
    uvm_report_cb::add(nullptr, &ctchr);

    // Promote UVM_ERROR -> UVM_FATAL with explicit UVM_DISPLAY | UVM_COUNT (no UVM_EXIT)
    UVM_ERROR("PROMOTE_FATAL", "promoted fatal without exit");

    // Promote UVM_WARNING -> UVM_ERROR with explicit UVM_DISPLAY (no UVM_COUNT)
    UVM_WARNING("PROMOTE_ERR", "promoted error without count");

    uvm_report_cb::do_delete(nullptr, &ctchr);

    // Check uvm_callbacks_base::m_tracing = false is preserved across report emission
    bool prev_tracing = uvm_callbacks_base::m_tracing;
    uvm_callbacks_base::m_tracing = false;
    UVM_INFO("TRC_CHK", "emit while m_tracing is false", UVM_LOW);
    bool tracing_after = uvm_callbacks_base::m_tracing;
    uvm_callbacks_base::m_tracing = prev_tracing;

    int quit_cnt = svr->get_quit_count();
    int err_cnt  = svr->get_severity_count(UVM_ERROR);
    int ftl_cnt  = svr->get_severity_count(UVM_FATAL);

    std::cout << "tracing_after=" << tracing_after
              << " quit_cnt=" << quit_cnt
              << " err_cnt=" << err_cnt
              << " ftl_cnt=" << ftl_cnt << std::endl;

    if (!tracing_after && quit_cnt == 1 && err_cnt == 1 && ftl_cnt == 1)
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
