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

// Regression test for uvm_config_db<T>:
// - m_rsc local-copy caching / resource reuse, r->wait_modified(), and
//   key delimiter uniqueness (("a","bc") vs ("ab","c"))
// - wait_modified() wildcard/regex field_name wakeups and waiter cleanup
// - exists() regex/glob lookup without get_record pollution or false SPELLCHK

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  int rsrc_woke;
  int cfg_woke;
  int reg_woke;
  int other_woke;

  test( uvm_component_name name )
    : uvm_test(name),
      rsrc_woke(0),
      cfg_woke(0),
      reg_woke(0),
      other_woke(0)
  {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // 5c: exists() with glob and regex field_names, before any get_by_name()
    uvm_config_db<int>::set(this, "env", "param_*", 7);
    uvm_config_db<int>::set(this, "env", "/mode_[0-9]+/", 8);

    bool ex_glob  = uvm_config_db<int>::exists(this, "env", "param_foo");
    bool ex_regex = uvm_config_db<int>::exists(this, "env", "mode_3");
    bool ex_spell = uvm_config_db<int>::exists(this, "env", "param_foo", true);
    bool ex_miss  = uvm_config_db<int>::exists(this, "env", "nonexistent", false);

    std::cout << "exists: glob=" << ex_glob
              << " regex=" << ex_regex
              << " spell=" << ex_spell
              << " miss=" << ex_miss << std::endl;

    // Must NOT pollute get_record
    uvm_resource_pool::get()->dump_get_records();

    if (!ex_glob || !ex_regex || !ex_spell || ex_miss)
      UVM_ERROR("EXISTS", "uvm_config_db::exists() regex/glob check failed");
  }

  void wait_rsrc_proc( uvm_resource<int>* r )
  {
    r->wait_modified();
    rsrc_woke++;
    std::cout << sc_core::sc_time_stamp() << ": rsrc wait_modified woke" << std::endl;
  }

  void wait_cfg_proc()
  {
    uvm_config_db<int>::wait_modified(this, "a", "cfg_alpha");
    cfg_woke++;
    std::cout << sc_core::sc_time_stamp() << ": cfg_alpha woke" << std::endl;
  }

  void wait_reg_proc()
  {
    uvm_config_db<int>::wait_modified(this, "a", "reg_01");
    reg_woke++;
    std::cout << sc_core::sc_time_stamp() << ": reg_01 woke" << std::endl;
  }

  void wait_other_proc()
  {
    uvm_config_db<int>::wait_modified(this, "b", "other");
    other_woke++;
    std::cout << sc_core::sc_time_stamp() << ": other woke" << std::endl;
  }

  void wait_loop_proc()
  {
    for (int i = 0; i < 50; ++i)
      uvm_config_db<int>::wait_modified(this, "loop", "iter_field");
    std::cout << sc_core::sc_time_stamp() << ": wait_loop completed 50 iterations" << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    uvm_resource_pool* rp = uvm_resource_pool::get();

    // 5a: m_rsc caching across distinct keys, reuse on override, delimiter uniqueness
    uvm_config_db<int>::set(this, "env", "a", 1);
    uvm_config_db<int>::set(this, "env", "b", 2);
    uvm_config_db<int>::set(this, "env", "a", 10);

    uvm_resource_types::rsrc_q_t* q_a =
      rp->lookup_name("test.env", "a", uvm_resource<int>::get_type(), false);
    int val_a = 0;
    uvm_config_db<int>::get(this, "env", "a", val_a);
    std::cout << "reuse: q_a_size=" << q_a->size() << " val_a=" << val_a << std::endl;
    if (q_a->size() != 1 || val_a != 10)
      UVM_ERROR("MRSC", "m_rsc failed to reuse existing resource");

    // Key delimiter uniqueness: ("a", "bc") vs ("ab", "c")
    uvm_config_db<int>::set(this, "a", "bc", 111);
    uvm_config_db<int>::set(this, "ab", "c", 222);
    int v_abc1 = 0, v_abc2 = 0;
    uvm_config_db<int>::get(this, "a", "bc", v_abc1);
    uvm_config_db<int>::get(this, "ab", "c", v_abc2);
    std::cout << "delim: v1=" << v_abc1 << " v2=" << v_abc2 << std::endl;
    if (v_abc1 != 111 || v_abc2 != 222)
      UVM_ERROR("DELIM", "m_rsc key delimiter collision");

    uvm_resource<int>* r_a = uvm_resource_db<int>::get_by_name("test.env", "a", false);
    if (r_a == nullptr)
      UVM_FATAL("RSRC", "get_by_name returned nullptr");

    // Advance past t=0 delta notifications before spawning r->wait_modified()
    sc_core::wait(1, sc_core::SC_NS);

    sc_core::sc_process_handle h_rsrc =
      sc_core::sc_spawn(sc_bind(&test::wait_rsrc_proc, this, r_a));
    sc_core::sc_process_handle h_cfg =
      sc_core::sc_spawn(sc_bind(&test::wait_cfg_proc, this));
    sc_core::sc_process_handle h_reg =
      sc_core::sc_spawn(sc_bind(&test::wait_reg_proc, this));
    sc_core::sc_process_handle h_other =
      sc_core::sc_spawn(sc_bind(&test::wait_other_proc, this));

    sc_core::wait(9, sc_core::SC_NS); // t = 10 ns
    uvm_config_db<int>::set(this, "env", "a", 20);
    sc_core::wait(1, sc_core::SC_NS);
    if (rsrc_woke != 1)
      UVM_ERROR("RSRC_WAIT", "r->wait_modified() did not wake on cached override");

    // 5b: wildcard and regex field_name wakeups
    sc_core::wait(9, sc_core::SC_NS); // t = 20 ns
    uvm_config_db<int>::set(this, "*", "cfg_*", 42);
    sc_core::wait(1, sc_core::SC_NS);
    if (cfg_woke != 1 || reg_woke != 0 || other_woke != 0)
      UVM_ERROR("WAIT_GLOB", "wildcard field_name wakeup mismatch");

    sc_core::wait(9, sc_core::SC_NS); // t = 30 ns
    uvm_config_db<int>::set(this, "*", "/reg_[0-9]+/", 99);
    sc_core::wait(1, sc_core::SC_NS);
    if (reg_woke != 1 || other_woke != 0)
      UVM_ERROR("WAIT_REGEX", "regex field_name wakeup mismatch");

    sc_core::wait(9, sc_core::SC_NS); // t = 40 ns
    uvm_config_db<int>::set(this, "b", "other", 7);
    sc_core::wait(1, sc_core::SC_NS);
    if (other_woke != 1)
      UVM_ERROR("WAIT_OTHER", "literal waiter failed to wake");

    // Repeated wait_modified/set cleanup loop
    sc_core::sc_process_handle h_loop =
      sc_core::sc_spawn(sc_bind(&test::wait_loop_proc, this));
    for (int i = 0; i < 50; ++i)
    {
      sc_core::wait(sc_core::SC_ZERO_TIME);
      uvm_config_db<int>::set(this, "loop", "iter_field", i);
      sc_core::wait(1, sc_core::SC_NS);
    }
    if (!h_loop.terminated())
      sc_core::wait(h_loop.terminated_event());

    // Process unwind cleanup test (kill while blocked in wait_modified)
    sc_core::sc_process_handle h_kill =
      sc_core::sc_spawn(sc_bind(&test::wait_other_proc, this));
    sc_core::wait(sc_core::SC_ZERO_TIME);
    h_kill.kill();
    sc_core::wait(1, sc_core::SC_NS);
    // Subsequent set on same field must not dereference destroyed waiter
    uvm_config_db<int>::set(this, "b", "other", 8);
    sc_core::wait(1, sc_core::SC_NS);

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
