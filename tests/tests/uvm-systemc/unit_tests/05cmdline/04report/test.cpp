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

// Test of the command line arguments which change the report settings
// of components: +uvm_set_verbosity for a phase and for a time,
// +uvm_set_action and +uvm_set_severity. Arguments which are not well
// formed are reported and ignored.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(comp);

  comp( uvm_component_name name ) : uvm_component(name) {}

  void messages( const std::string& where )
  {
    UVM_INFO("FULL", where + ": verbosity UVM_FULL", UVM_FULL);
    UVM_INFO("MYID", where + ": verbosity UVM_HIGH", UVM_HIGH);
    UVM_INFO("NOISY", where + ": verbosity UVM_NONE", UVM_NONE);
    UVM_WARNING("WARN", where + ": warning");
    UVM_ERROR("DEMOTE", where + ": error");
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_component::build_phase(phase);
    messages("build");
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    messages("connect");
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    messages("run");
    sc_core::wait(20, sc_core::SC_NS);
    messages("run after 20 ns");
    phase.drop_objection(this);
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    comp::type_id::create("c1", this);
    comp::type_id::create("c2", this);
    comp::type_id::create("c3", this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    std::cout << "simulation ended at " << sc_core::sc_time_stamp() << std::endl;
    std::cout << "quit count = "
              << uvm_report_server::get_server()->get_quit_count() << std::endl;
    std::cout << "** UVM TEST PASSED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}

int main(int, char*[])
{
  const char* argv[] = {
    "test",
    "+UVM_VERBOSITY=bogus",
    "+UVM_MAX_QUIT_COUNT=100",
    // c1: everything from construction on, no NOISY, errors demoted
    "+uvm_set_verbosity=test.c1,_ALL_,UVM_FULL,build",
    "+uvm_set_action=test.c1,NOISY,UVM_INFO,UVM_NO_ACTION",
    "+uvm_set_severity=test.c1,DEMOTE,UVM_ERROR,UVM_WARNING",
    // c2: one id from the run phase on, warnings become counted infos
    "+uvm_set_verbosity=test.c2,MYID,UVM_HIGH,run",
    "+uvm_set_severity=test.c2,_ALL_,UVM_WARNING,UVM_INFO",
    "+uvm_set_action=test.c2,WARN,_ALL_,UVM_DISPLAY|UVM_COUNT",
    // c3: verbosity raised at 10 ns; the second setting is after the
    // end of the test and shall not extend the simulation
    "+uvm_set_verbosity=test.c3,_ALL_,UVM_HIGH,time,10",
    "+uvm_set_verbosity=test.c3,_ALL_,UVM_NONE,time,1ms",
    "+uvm_set_action=test.c3,_ALL_,UVM_ERROR,UVM_NO_ACTION",
    // not well formed
    "+uvm_set_verbosity=test.c1,_ALL_,UVM_BOGUS,build",
    "+uvm_set_verbosity=test.c1,_ALL_,UVM_LOW",
    "+uvm_set_verbosity=test.c1,_ALL_,UVM_LOW,run,10",
    "+uvm_set_verbosity=test.c1,_ALL_,UVM_LOW,time",
    "+uvm_set_verbosity=test.c1,_ALL_,UVM_LOW,time,soon",
    "+uvm_set_action=test.c1,NOISY,UVM_BAD,UVM_DISPLAY",
    "+uvm_set_action=test.c1,NOISY,UVM_INFO,UVM_SHOUT",
    "+uvm_set_severity=test.c1,DEMOTE,UVM_ERROR",
    "+uvm_set_verbosity=,_ALL_,UVM_LOW,run",
    "+uvm_set_action=/[/,NOISY,UVM_INFO,UVM_NO_ACTION",
    "+uvm_set_verbosity=test.c1,_ALL_,UVM_LOW,time,0x20"
  };
  int argc = sizeof(argv) / sizeof(argv[0]);

  return sc_core::sc_elab_and_sim(argc, const_cast<char**>(argv));
}
