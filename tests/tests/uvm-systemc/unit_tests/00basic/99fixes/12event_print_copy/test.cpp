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
// state, the trigger time and the trigger data, but not the
// callbacks: the event owns its callbacks and deletes them in
// delete_callback(), so a copy must not hold them. copy() reports no
// DOCPY warning, and a copy which turns the event on or off resumes the
// processes waiting in wait_on() or wait_off(), as in UVM-SV.

#include <systemc>
// included first so that uvm_event::add_callback() takes a
// uvm::uvm_event_callback with every set of global declarations
#include "uvmsc/base/uvm_event_callback.h"
#include <uvm>

#include <iostream>

using namespace uvm;

class data_obj : public uvm::uvm_object
{
 public:
  UVM_OBJECT_UTILS(data_obj);
  data_obj( const std::string& name = "data_obj" ) : uvm::uvm_object(name) {}
};

class count_cb : public uvm::uvm_event_callback
{
 public:
  int* count;
  count_cb( const std::string& name, int* cnt ) : uvm::uvm_event_callback(name), count(cnt) {}
  virtual const std::string get_type_name() const { return "count_cb"; }
  virtual void post_trigger( const uvm::uvm_event* e, const uvm::uvm_object* data = nullptr ) const
  { (*count)++; }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_event ev;
  data_obj data;

  test( uvm_component_name name )
  : uvm_test(name), ev("ev"), data("data"), on_waited("on_waited"),
    off_waited("off_waited"), on_woke(false), off_woke(false) {}

  uvm_event on_waited, off_waited;
  bool on_woke, off_woke;

  void wait_on_proc() { on_waited.wait_on(); on_woke = true; }
  void wait_off_proc() { off_waited.wait_off(); off_woke = true; }

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

    // callbacks stay with the event they are registered with
    int kept_cnt = 0, deleted_cnt = 0;
    uvm_event src("src");
    count_cb* kept = new count_cb("kept", &kept_cnt);
    count_cb* deleted = new count_cb("deleted", &deleted_cnt);
    src.add_callback(kept);
    src.add_callback(deleted);

    uvm_event cb_copy("cb_copy");
    cb_copy.copy(src);
    src.delete_callback(deleted); // deletes the callback object

    std::cout << "--- event with callbacks" << std::endl;
    src.print();
    std::cout << "--- copy of an event with callbacks" << std::endl;
    cb_copy.print();

    cb_copy.trigger();
    check(kept_cnt == 0, "copy does not call the callbacks of the original");
    src.trigger();
    check(kept_cnt == 1, "original calls its callback");

    // copy() is implemented: no DOCPY warning of uvm_object::do_copy
    set_report_verbosity_level(UVM_HIGH);
    uvm_root::get()->set_report_verbosity_level(UVM_HIGH);
    uvm_event quiet("quiet");
    quiet.copy(src);
    uvm_report_server* svr = uvm_coreservice_t::get()->get_report_server();
    check(svr->get_id_count("DOCPY") == 0, "copy reports no DOCPY warning");
    set_report_verbosity_level(UVM_MEDIUM);
    uvm_root::get()->set_report_verbosity_level(UVM_MEDIUM);

    // the processes waiting for the state taken over by copy() resume
    uvm_event off_ev("off_ev");
    off_waited.trigger();
    sc_core::sc_spawn(sc_bind(&test::wait_on_proc, this));
    sc_core::sc_spawn(sc_bind(&test::wait_off_proc, this));
    sc_core::wait(sc_core::SC_ZERO_TIME);
    on_waited.copy(src);
    off_waited.copy(off_ev);
    sc_core::wait(sc_core::SC_ZERO_TIME);
    check(on_woke, "copy which turns the event on resumes wait_on()");
    check(off_woke, "copy which turns the event off resumes wait_off()");

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
