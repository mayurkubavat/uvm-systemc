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

// Test of uvm_phase_cb and uvm_phase_state_change: a callback added for
// all phases and a callback added for one phase node are called for each
// state change of the phases, including a phase jump.

#include <systemc>
#include <uvm>

#include <iostream>
#include <set>
#include <string>

using namespace uvm;

class my_phase_cb : public uvm_phase_cb
{
 public:
  my_phase_cb( const std::string& name, const std::set<std::string>& phases )
  : uvm_phase_cb(name), m_phases(phases) {}

  virtual void phase_state_change( uvm_phase* phase, uvm_phase_state_change* change )
  {
    if (!m_phases.empty() && m_phases.find(phase->get_name()) == m_phases.end())
      return;

    std::cout << sc_core::sc_time_stamp() << ": " << get_name() << " "
              << phase->get_name() << " "
              << uvm_phase_state_name[change->get_prev_state()] << " -> "
              << uvm_phase_state_name[change->get_state()];
    if (change->jump_to() != nullptr)
      std::cout << " (jump to " << change->jump_to()->get_name() << ")";
    if (change->get_state() != phase->get_state())
      std::cout << " ERROR: state mismatch";
    std::cout << std::endl;
  }

 private:
  std::set<std::string> m_phases;
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  bool jump_once;
  my_phase_cb* main_cb;
  my_phase_cb* disabled_cb;

  test( uvm_component_name name )
  : uvm_test(name), jump_once(true), main_cb(nullptr), disabled_cb(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // a callback for the main phase node of the uvm domain only
    uvm_phase* main_node = uvm_domain::get_uvm_domain()->find(uvm_main_phase::get());
    main_cb = new my_phase_cb("main_cb", std::set<std::string>());
    uvm_phase_cb_pool::add(main_node, main_cb);

    // a disabled callback is not called
    disabled_cb = new my_phase_cb("disabled_cb", std::set<std::string>());
    uvm_phase_cb_pool::add(main_node, disabled_cb);
    disabled_cb->callback_mode(0);
  }

  virtual void pre_main_phase( uvm_phase& phase )
  {
    if (jump_once)
    {
      jump_once = false;
      phase.raise_objection(this);
      std::cout << sc_core::sc_time_stamp() << ": jumping back to reset" << std::endl;
      phase.jump(uvm_reset_phase::get());
    }
  }

  virtual void main_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    sc_core::wait(10, sc_core::SC_NS);
    phase.drop_objection(this);
  }

  virtual void shutdown_phase( uvm_phase& phase )
  {
    // removed again: main_cb is not called for the main phase anymore
    uvm_phase_cb_pool::do_delete(uvm_domain::get_uvm_domain()->find(uvm_main_phase::get()), main_cb);
    std::cout << sc_core::sc_time_stamp() << ": main_cb deleted" << std::endl;
  }
};

int sc_main(int, char*[])
{
  std::set<std::string> phases;
  phases.insert("build");
  phases.insert("reset");
  phases.insert("pre_main");
  phases.insert("run");

  // a callback for all phases
  my_phase_cb all_cb("all_cb", phases);
  uvm_phase_cb_pool::add(nullptr, &all_cb);

  run_test("test");
  return 0;
}
