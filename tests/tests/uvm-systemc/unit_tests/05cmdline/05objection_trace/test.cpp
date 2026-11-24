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

// Test of the command line argument +UVM_OBJECTION_TRACE. It turns on
// the trace mode of every objection, so that each raise and drop of a
// phase objection is reported with the id OBJTN_TRC. This includes the
// objections which exist before the arguments are available.

#include <systemc>
#include <uvm>

#include <iostream>

using namespace uvm;

// created before the command line arguments are available
uvm_objection early_objection("early_objection");

class comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(comp);

  comp( uvm_component_name name ) : uvm_component(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this, "comp is busy");
    sc_core::wait(10, sc_core::SC_NS);
    phase.drop_objection(this, "comp is done");
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
  }

  virtual void run_phase( uvm_phase& phase )
  {
    // an objection created by the user is traced as well
    uvm_objection objection("my_objection");
    std::cout << "trace mode of my_objection = "
              << objection.trace_mode() << std::endl;
    std::cout << "trace mode of early_objection = "
              << early_objection.trace_mode() << std::endl;

    phase.raise_objection(this);
    sc_core::wait(20, sc_core::SC_NS);
    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    std::cout << "simulation ended at " << sc_core::sc_time_stamp() << std::endl;
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
    "+UVM_OBJECTION_TRACE"
  };
  int argc = sizeof(argv) / sizeof(argv[0]);

  return sc_core::sc_elab_and_sim(argc, const_cast<char**>(argv));
}
