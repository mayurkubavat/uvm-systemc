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

// Test of the command line arguments which are applied when a test is
// started: +UVM_VERBOSITY, +uvm_set_config_int, +uvm_set_config_string,
// +uvm_set_type_override, +uvm_set_inst_override, +UVM_TIMEOUT,
// +UVM_MAX_QUIT_COUNT and +UVM_DUMP_CMDLINE_ARGS.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class comp_base : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(comp_base);

  comp_base( uvm_component_name name ) : uvm_component(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_component::build_phase(phase);
    std::cout << get_full_name() << " is a " << get_type_name() << std::endl;
    UVM_INFO("VERB", "message with verbosity UVM_HIGH", UVM_HIGH);
    UVM_INFO("VERB", "message with verbosity UVM_FULL", UVM_FULL);
  }
};

class comp_ovr : public comp_base
{
 public:
  UVM_COMPONENT_UTILS(comp_ovr);
  comp_ovr( uvm_component_name name ) : comp_base(name) {}
};

class comp_inst_ovr : public comp_base
{
 public:
  UVM_COMPONENT_UTILS(comp_inst_ovr);
  comp_inst_ovr( uvm_component_name name ) : comp_base(name) {}
};

class env : public uvm_env
{
 public:
  UVM_COMPONENT_UTILS(env);

  env( uvm_component_name name ) : uvm_env(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    int count = 0;
    int width = 0;
    int depth = 0;
    int unset = 0;
    uvm_bitstream_t bits = 0;
    std::string mode;

    uvm_env::build_phase(phase);

    if (!uvm_config_db<int>::get(this, "", "count", count))
      UVM_ERROR("TEST", "count not found");
    if (!uvm_config_db<int>::get(this, "", "width", width))
      UVM_ERROR("TEST", "width not found");
    if (!uvm_config_db<int>::get(this, "", "depth", depth))
      UVM_ERROR("TEST", "depth not found");
    if (!uvm_config_int::get(this, "", "count", bits))
      UVM_ERROR("TEST", "count not found as uvm_bitstream_t");
    if (!uvm_config_string::get(this, "", "mode", mode))
      UVM_ERROR("TEST", "mode not found");
    if (uvm_config_db<int>::get(this, "", "bad", unset))
      UVM_ERROR("TEST", "invalid setting has been applied");

    // a value which does not fit an int is stored as uvm_bitstream_t only
    if (uvm_config_db<int>::get(this, "", "wide", unset))
      UVM_ERROR("TEST", "wide value has been stored as int");
    if (!uvm_config_int::get(this, "", "wide", bits))
      UVM_ERROR("TEST", "wide not found as uvm_bitstream_t");
    std::cout << "wide as uvm_bitstream_t = " << bits.to_uint64() << std::endl;
    uvm_config_int::get(this, "", "count", bits);

    std::cout << "count = " << count << std::endl;
    std::cout << "count as uvm_bitstream_t = " << bits.to_uint64() << std::endl;
    std::cout << "width = " << width << std::endl;
    std::cout << "depth = " << depth << std::endl;
    std::cout << "mode = " << mode << std::endl;

    comp_base::type_id::create("c1", this);
    comp_base::type_id::create("c2", this);
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

    // the command line takes precedence over settings of the test
    uvm_config_db<int>::set(this, "env", "count", 99);
    uvm_config_db<std::string>::set(this, "env", "mode", "slow");

    env::type_id::create("env", this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    // both are set to not overridable on the command line
    uvm_root::get()->set_timeout(sc_core::sc_time(1, sc_core::SC_SEC));
    uvm_report_server::get_server()->set_max_quit_count(3);

    std::cout << "max quit count = "
              << uvm_report_server::get_server()->get_max_quit_count() << std::endl;
    std::cout << "** UVM TEST PASSED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  uvm_config_db<int>::set(nullptr, "test.env", "width", 1);

  run_test("test");
  return 0;
}

int main(int, char*[])
{
  const char* argv[] = {
    "test",
    "+UVM_DUMP_CMDLINE_ARGS",
    "+UVM_VERBOSITY=UVM_HIGH",
    "+UVM_VERBOSITY=UVM_LOW",
    "+uvm_set_config_int=test.env,count,'h10",
    "+uvm_set_config_int=*,width,-3",
    "+UVM_SET_CONFIG_INT=test.env,depth,0b101",
    "+uvm_set_config_int=test.env,bad",
    "+uvm_set_config_int=test.env,bad,12zz",
    "+uvm_set_config_int=test.env,wide,0xFFFFFFFFFF",
    "+uvm_set_config_string=test.env,mode,fast",
    "+uvm_set_config_string=test.env",
    "+uvm_set_type_override=comp_base,comp_ovr",
    "+uvm_set_type_override=comp_base,comp_ovr,2",
    "+uvm_set_inst_override=comp_base,comp_inst_ovr,test.env.c2",
    "+uvm_set_inst_override=comp_base,comp_inst_ovr",
    "+uvm_set_inst_override=comp_base,comp_inst_ovr,",
    "+UVM_TIMEOUT=50,NO",
    "+UVM_TIMEOUT=nan",
    "+UVM_MAX_QUIT_COUNT=7,NO"
  };
  int argc = sizeof(argv) / sizeof(argv[0]);

  return sc_core::sc_elab_and_sim(argc, const_cast<char**>(argv));
}
