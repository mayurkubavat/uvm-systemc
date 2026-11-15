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

// Test of uvm_heartbeat: components show that they are alive by raising
// and dropping an objection between the triggers of an event. The
// heartbeat reports HBFAIL when the activity does not fit its mode.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>
#include <vector>

using namespace uvm;

// Turns the fatal HBFAIL into a warning, so that the test can go on
class hb_catcher : public uvm_report_catcher
{
 public:
  int seen;

  hb_catcher() : uvm_report_catcher("hb_catcher"), seen(0) {}

  virtual action_e do_catch()
  {
    if (get_id() == "HBFAIL")
    {
      seen++;
      set_severity(UVM_WARNING);
      set_action(UVM_DISPLAY);
    }
    return THROW;
  }
};

class comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(comp);

  uvm_objection* hb;
  bool active;

  comp( uvm_component_name name ) : uvm_component(name), hb(nullptr), active(true) {}

  // shows activity once in every window of 10 ns
  virtual void run_phase( uvm_phase& phase )
  {
    sc_core::wait(5, sc_core::SC_NS);

    while (true)
    {
      if (active)
      {
        hb->raise_objection(this);
        hb->drop_objection(this);
      }
      sc_core::wait(10, sc_core::SC_NS);
    }
  }
};

uvm_heartbeat* late_hb = nullptr;
uvm_event* late_event = nullptr;

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_objection hb_objection;
  uvm_event hb_event;
  uvm_heartbeat hb;
  hb_catcher catcher;
  comp* c[3];

  test( uvm_component_name name )
  : uvm_test(name), hb_objection("hb_objection"), hb_event("hb_event"),
    hb("hb", this, &hb_objection)
  {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    for (int i = 0; i < 3; i++)
    {
      c[i] = comp::type_id::create("c" + std::to_string(i), this);
      c[i]->hb = &hb_objection;
    }

    uvm_report_cb::add(nullptr, &catcher);
  }

  // lets 'windows' heartbeat windows of 10 ns pass and reports the failures
  void run( const std::string& what, bool a0, bool a1, bool a2, int windows = 2 )
  {
    c[0]->active = a0; c[1]->active = a1; c[2]->active = a2;

    int before = catcher.seen;
    std::cout << sc_core::sc_time_stamp() << ": " << what << std::endl;

    for (int i = 0; i < windows; i++)
    {
      sc_core::wait(10, sc_core::SC_NS);
      hb_event.trigger();
      // let the heartbeat check this window before the next one starts
      sc_core::wait(sc_core::SC_ZERO_TIME);
    }

    std::cout << "  failures: " << (catcher.seen - before) << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    std::vector<uvm_component*> comps;
    comps.push_back(c[0]);
    comps.push_back(c[1]);

    std::cout << "a warning for a start without event is expected" << std::endl;
    hb.start();

    // watch c0 and c1, then c2 as well
    hb.set_heartbeat(&hb_event, comps);
    hb.add(c[2]);

    std::cout << "default mode is UVM_ALL_ACTIVE: " << (hb.set_mode() == UVM_ALL_ACTIVE) << std::endl;

    run("all active, all three alive", true, true, true, 3);
    run("all active, c2 silent", true, true, false);

    hb.set_mode(UVM_ANY_ACTIVE);
    run("any active, only c1 alive", false, true, false);
    run("any active, nobody alive", false, false, false);

    hb.set_mode(UVM_ONE_ACTIVE);
    run("one active, only c0 alive", true, false, false);
    run("one active, c0 and c1 alive", true, true, false);
    run("one active, nobody alive", false, false, false);

    // a component which is removed is not expected any more
    hb.set_mode(UVM_ALL_ACTIVE);
    hb.remove(c[2]);
    run("all active without c2, c2 silent", true, true, false);

    // no checks while stopped
    hb.stop();
    run("stopped, nobody alive", false, false, false);

    // another event while running is an error, after a stop it is fine
    uvm_event other("other");
    hb.start();
    std::cout << "an error for a different event is expected" << std::endl;
    hb.start(&other);
    run("started again, c0 and c1 alive", true, true, false);

    std::cout << "heartbeat failures in total: " << catcher.seen << std::endl;

    hb.stop();

    // stopping a heartbeat whose process did not get to wait for the
    // event leaves the other processes which wait for it alone
    late_event = new uvm_event("late_event");
    late_hb = new uvm_heartbeat("late_hb", this);
    sc_core::sc_spawn(sc_bind(&uvm_event::wait_trigger, late_event));
    sc_core::wait(1, sc_core::SC_NS);
    late_hb->start(late_event);
    late_hb->stop();
    std::cout << "waiting for the event after start and stop: "
              << late_event->get_num_waiters() << std::endl;

    // this one is still started when the simulation ends
    late_hb->start(late_event);
    sc_core::wait(1, sc_core::SC_NS);

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[])
{
  run_test("test");

  // a heartbeat can be deleted after the simulation, also a started one
  delete late_hb;
  delete late_event;
  std::cout << "heartbeat deleted after the simulation" << std::endl;
  return 0;
}
