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

// Regression test for the is_active setting of uvm_agent. As for a
// field registered with uvm_field_enum, the setting may be given as a
// uvm_active_passive_enum, an integral value or a string holding the
// name of the enumerator. A string which is no enumerator name is not
// a valid setting: it is skipped, and a setting of lower precedence
// applies, if any.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class my_agent : public uvm_agent
{
 public:
  UVM_COMPONENT_UTILS(my_agent);

  my_agent( uvm_component_name name ) : uvm_agent(name) {}
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  my_agent* agents[8];
  uvm_active_passive_enum expected[8];

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    uvm_config_db<uvm_active_passive_enum>::set(this, "a0", "is_active", UVM_PASSIVE);
    uvm_config_db<int>::set(this, "a1", "is_active", (int)UVM_PASSIVE);
    uvm_config_db<std::string>::set(this, "a2", "is_active", "UVM_PASSIVE");
    uvm_config_db<std::string>::set(this, "a3", "is_active", "UVM_ACTIVE");
    uvm_config_db<std::string>::set(this, "a4", "is_active", "PASSIVE");
    uvm_config_db<std::string>::set(this, "a5", "is_active", "uvm_passive");

    // a6 and a7 also have a setting of higher precedence from sc_main
    uvm_config_db<int>::set(this, "a6", "is_active", (int)UVM_ACTIVE);
    uvm_config_db<int>::set(this, "a7", "is_active", (int)UVM_PASSIVE);

    uvm_active_passive_enum exp[8] =
      { UVM_PASSIVE, UVM_PASSIVE, UVM_PASSIVE, UVM_ACTIVE,
        UVM_ACTIVE, UVM_ACTIVE, UVM_PASSIVE, UVM_PASSIVE };

    for (int i = 0; i < 8; i++)
    {
      agents[i] = my_agent::type_id::create("a" + std::to_string(i), this);
      expected[i] = exp[i];
    }
  }

  virtual void end_of_elaboration_phase( uvm_phase& phase )
  {
    for (int i = 0; i < 8; i++)
    {
      uvm_active_passive_enum act = agents[i]->get_is_active();
      std::cout << agents[i]->get_full_name() << ": "
                << (act == UVM_ACTIVE ? "UVM_ACTIVE" : "UVM_PASSIVE") << std::endl;
      if (act != expected[i])
        UVM_ERROR("ISACTIVE", "unexpected is_active for " + agents[i]->get_full_name());
    }
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
  uvm_config_db<std::string>::set(nullptr, "test.a6", "is_active", "UVM_PASSIVE");
  uvm_config_db<std::string>::set(nullptr, "test.a7", "is_active", "bogus");

  run_test("test");
  return 0;
}
