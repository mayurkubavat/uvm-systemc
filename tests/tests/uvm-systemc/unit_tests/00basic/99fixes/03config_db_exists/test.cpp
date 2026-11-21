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

// Regression test for uvm_config_db<T>::exists(). For a plain field
// name it must report a setting whenever uvm_config_db<T>::get() with
// the same context and instance name finds one, and must not report
// settings of another name or type.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

template <typename T>
void check( uvm_component* cntxt,
            const std::string& inst_name,
            const std::string& field_name,
            bool expected )
{
  T value = T();
  bool found = uvm_config_db<T>::get(cntxt, inst_name, field_name, value);
  bool exists = uvm_config_db<T>::exists(cntxt, inst_name, field_name);

  std::cout << "context '" << (cntxt ? cntxt->get_full_name() : "")
            << "' inst '" << inst_name << "' field '" << field_name
            << "': get=" << found << " exists=" << exists << std::endl;

  if (found != expected)
    UVM_ERROR("Test", "get() result is not as expected for field '" + field_name + "'");
  if (exists != expected)
    UVM_ERROR("Test", "exists() result is not as expected for field '" + field_name + "'");
}

class my_env : public uvm_env
{
 public:
  UVM_COMPONENT_UTILS(my_env);

  my_env( uvm_component_name name ) : uvm_env(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_env::build_phase(phase);

    // settings made by the parent, seen from the component they apply to
    check<int>(this, "", "from_test", true);
    check<int>(this, "", "for_all", true);
    check<int>(nullptr, get_full_name(), "from_test", true);
    check<int>(nullptr, get_full_name(), "from_top", true);

    // other name, other type
    check<int>(this, "", "from_tset", false);
    check<std::string>(this, "", "from_test", false);
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  my_env* env;

  test( uvm_component_name name ) : uvm_test(name), env(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    uvm_config_db<int>::set(this, "env", "from_test", 1);
    uvm_config_db<int>::set(this, "*", "for_all", 2);
    uvm_config_db<int>::set(this, "", "for_test", 3);

    // relative to this component
    check<int>(this, "env", "from_test", true);
    check<int>(this, "env", "for_all", true);
    check<int>(this, "", "for_test", true);
    check<int>(this, "env", "for_test", false);

    // absolute, without a context
    check<int>(nullptr, "test.env", "from_top", true);
    check<int>(nullptr, "test.env", "from_test", true);
    check<int>(nullptr, "test.other", "from_top", false);
    check<int>(nullptr, "", "at_top", true);

    env = my_env::type_id::create("env", this);
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
  uvm_config_db<int>::set(nullptr, "test.env", "from_top", 4);
  uvm_config_db<int>::set(nullptr, "", "at_top", 5);

  run_test("test");

  return 0;
}
