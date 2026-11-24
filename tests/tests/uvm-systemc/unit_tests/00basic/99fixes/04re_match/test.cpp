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

// Regression test for regular expressions in the configuration
// database. A regular expression scope, written between slashes, is
// searched in the instance name as in UVM-SV, so it does not need to
// match the whole name. wait_modified() returns only for settings which
// apply to the instance it waits for, also when the setting is made
// relative to a context. An empty scope matches the empty name only.

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
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  void lookup( const std::string& inst, const std::string& field )
  {
    int value = 0;
    bool found = uvm_config_db<int>::get(nullptr, inst, field, value);
    std::cout << "get '" << field << "' in '" << inst << "': "
              << (found ? std::to_string(value) : std::string("not found")) << std::endl;
  }

  uvm_component* a;
  uvm_component* b;

  void waiter()
  {
    uvm_config_db<int>::wait_modified(nullptr, "test.a", "mode");
    std::cout << sc_core::sc_time_stamp() << ": test.a was notified" << std::endl;
  }

  void waiter_a()
  {
    uvm_config_db<int>::wait_modified(a, "", "speed");
    std::cout << sc_core::sc_time_stamp() << ": test.a was notified of speed" << std::endl;
  }

  void waiter_b()
  {
    uvm_config_db<int>::wait_modified(this, "b", "speed");
    std::cout << sc_core::sc_time_stamp() << ": test.b was notified of speed" << std::endl;
  }

  void waiter_top()
  {
    uvm_config_db<int>::wait_modified(nullptr, "", "level");
    std::cout << sc_core::sc_time_stamp() << ": uvm_top was notified of level" << std::endl;
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    a = comp::type_id::create("a", this);
    b = comp::type_id::create("b", this);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // regular expressions, searched in the instance name
    uvm_config_db<int>::set(nullptr, "/env/", "in_env", 1);
    uvm_config_db<int>::set(nullptr, "/^test\\.env$/", "only_env", 2);
    uvm_config_db<int>::set(nullptr, "/agent[0-9]/", "agent", 3);

    lookup("test.env", "in_env");
    lookup("test.env.agent0", "in_env");
    lookup("test.other", "in_env");
    lookup("test.env", "only_env");
    lookup("test.env.agent0", "only_env");
    lookup("test.env.agent1.drv", "agent");
    lookup("test.env.agentx", "agent");

    // globs still match the whole name
    uvm_config_db<int>::set(nullptr, "test.env*", "glob", 4);
    lookup("test.env.agent0", "glob");
    lookup("top.test.env", "glob");

    // wait_modified() wakes up for a setting of its own instance only
    sc_core::sc_spawn(sc_bind(&test::waiter, this));
    sc_core::wait(10, sc_core::SC_NS);
    uvm_config_db<int>::set(nullptr, "test.b", "mode", 1);
    sc_core::wait(10, sc_core::SC_NS);
    uvm_config_db<int>::set(nullptr, "test.a", "mode", 2);
    sc_core::wait(10, sc_core::SC_NS);

    // a setting relative to a context wakes the waiters of that instance
    sc_core::sc_spawn(sc_bind(&test::waiter_a, this));
    sc_core::sc_spawn(sc_bind(&test::waiter_b, this));
    sc_core::wait(10, sc_core::SC_NS);
    uvm_config_db<int>::set(this, "a", "speed", 1);
    sc_core::wait(10, sc_core::SC_NS);
    uvm_config_db<int>::set(nullptr, "test.*", "speed", 2);
    sc_core::wait(10, sc_core::SC_NS);

    // a setting for uvm_top wakes the waiters for uvm_top
    sc_core::sc_spawn(sc_bind(&test::waiter_top, this));
    sc_core::wait(10, sc_core::SC_NS);
    uvm_config_db<int>::set(nullptr, "", "level", 1);
    sc_core::wait(10, sc_core::SC_NS);

    // an empty scope is the empty name only
    uvm_resource_db<int>::set("", "top_only", 7);
    std::cout << "resource with empty scope found in '': "
              << (uvm_resource_db<int>::get_by_name("", "top_only", false) != nullptr)
              << ", in 'test.a': "
              << (uvm_resource_db<int>::get_by_name("test.a", "top_only", false) != nullptr)
              << std::endl;

    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    std::cout << "** UVM TEST PASSED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
