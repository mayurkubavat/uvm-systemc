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

// A component which is deleted before the test runs leaves no trace in
// the phase schedules and in the component hierarchy: its schedule
// stays with its domain, and its parent forgets it. A child which
// outlives its parent can still be deleted. Run with a memory
// checker (AddressSanitizer, or MallocScribble=1 on macOS) to see the
// difference; without one, reading freed memory may go unnoticed.

#include <systemc>
#include <uvm>

#include <iostream>

using namespace uvm;

class comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(comp);

  int delay;

  comp( uvm_component_name name, int delay_ = 10 ) : uvm_component(name), delay(delay_) {}

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    sc_core::wait(delay, sc_core::SC_NS);
    UVM_INFO("RUN", "run phase of " + get_full_name() + " done", UVM_NONE);
    phase.drop_objection(this);
  }
};

// a component which creates a child, and does not delete it
class holder : public uvm_component
{
 public:
  comp* child;

  holder( uvm_component_name name ) : uvm_component(name)
  {
    child = new comp("child");
  }
};

int sc_main(int, char*[])
{
  uvm_domain* d1 = new uvm_domain("d1");
  uvm_domain* d2 = new uvm_domain("d2");

  // a component with its own schedule in d1, deleted again
  comp* gone = new comp("gone");
  gone->set_domain(*d1);
  delete gone;

  // components which stay, in d1 and in d2
  comp* kept1 = new comp("kept1");
  kept1->set_domain(*d1);
  comp* kept2 = new comp("kept2", 20);
  kept2->set_domain(*d2);

  // a parent which is deleted before its child
  holder* parent = new holder("parent");
  comp* child = parent->child;
  delete parent;
  delete child;
  std::cout << "parent and child deleted" << std::endl;

  std::cout << "children of uvm_top: " << uvm_root::get()->get_num_children() << std::endl;

  run_test();

  return 0;
}
