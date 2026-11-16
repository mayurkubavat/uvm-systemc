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

// Regression test for uvm_event::do_print() and do_copy(). Printing an
// event must show its state, and copy() must take over the on/off
// state, the trigger time and the trigger data.

#include <systemc>
#include <uvm>

#include <iostream>

using namespace uvm;

class data_obj : public uvm::uvm_object
{
 public:
  UVM_OBJECT_UTILS(data_obj);
  data_obj( const std::string& name = "data_obj" ) : uvm::uvm_object(name) {}
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_event ev;
  data_obj data;

  test( uvm_component_name name ) : uvm_test(name), ev("ev"), data("data") {}

  void check( bool cond, const std::string& what )
  {
    std::cout << what << ": " << (cond ? "ok" : "MISMATCH") << std::endl;
    if (!cond)
      UVM_ERROR("EVCOPY", what);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    std::cout << "--- untriggered event" << std::endl;
    ev.print();

    sc_core::wait(10, sc_core::SC_NS);
    ev.trigger(&data);

    std::cout << "--- triggered event" << std::endl;
    ev.print();

    uvm_event cp("cp");
    cp.copy(ev);

    std::cout << "--- copy" << std::endl;
    cp.print();

    check(cp.is_on(), "copy is on");
    check(cp.get_trigger_time() == sc_core::sc_time(10, sc_core::SC_NS),
          "copy trigger time");
    check(cp.get_trigger_data() == &data, "copy trigger data");
    check(cp.get_name() == "cp", "copy keeps its name");

    ev.reset();
    cp.copy(ev);
    check(cp.is_off(), "copy of a reset event is off");

    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    uvm_coreservice_t* cs = uvm_coreservice_t::get();
    uvm_report_server* svr = cs->get_report_server();

    if (svr->get_severity_count(UVM_FATAL) +
        svr->get_severity_count(UVM_ERROR) == 0)
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
