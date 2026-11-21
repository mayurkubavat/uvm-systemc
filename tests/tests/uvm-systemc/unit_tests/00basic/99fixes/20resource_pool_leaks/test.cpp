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

// Regression test for uvm_resource_pool memory leaks and lookup APIs:
// - 10,000-iteration loop of get_by_name, get_by_type, sort_by_precedence,
//   uvm_config_db::get, uvm_config_db::exists, wait_modified, and
//   print_resources(nullptr) with live heap growth check on Apple non-ASan.
// - Functional coverage of lookup_name, lookup_type, lookup_regex_names,
//   lookup_regex, lookup_scope, find_unused_resources, uvm_agent,
//   uvm_reg_block::find_blocks, and uvm_component::print_config.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>
#include <vector>

#if defined(__APPLE__)
#  if defined(__has_feature)
#    if !__has_feature(address_sanitizer)
#      include <malloc/malloc.h>
#      define UVM_TEST_CHECK_MALLOC_ZONE 1
#    endif
#  else
#    include <malloc/malloc.h>
#    define UVM_TEST_CHECK_MALLOC_ZONE 1
#  endif
#endif

using namespace uvm;

class leak_agent : public uvm_agent
{
 public:
  UVM_COMPONENT_UTILS(leak_agent);

  leak_agent( uvm_component_name name ) : uvm_agent(name) {}
};

class leak_reg_block : public uvm_reg_block
{
 public:
  leak_reg_block( const std::string& name = "leak_blk" )
    : uvm_reg_block(name, UVM_NO_COVERAGE)
  {}
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  leak_agent* ag;
  leak_reg_block* blk;

  test( uvm_component_name name )
    : uvm_test(name), ag(nullptr), blk(nullptr)
  {}

  virtual ~test()
  {
    delete blk;
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    uvm_config_db<int>::set(this, "ag", "is_active", (int)UVM_PASSIVE);
    ag = leak_agent::type_id::create("ag", this);

    blk = new leak_reg_block("leak_blk");
    blk->configure(nullptr, "top.blk");
    blk->lock_model();
  }

  void wait_loop_proc()
  {
    for (int i = 0; i < 200; ++i)
      uvm_config_db<int>::wait_modified(this, "ag", "dyn_cfg");
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    uvm_coreservice_t* cs = uvm_coreservice_t::get();
    uvm_root* top = cs->get_root();
    uvm_resource_pool* rp = uvm_resource_pool::get();

    // Populate resources (both literal and unused)
    uvm_config_db<int>::set(this, "ag", "k_used", 42);
    uvm_config_db<int>::set(this, "ag", "k_unused", 99);

    // Exercise lookup_* before wildcard names exist (m_has_wildcard_names == false)
    uvm_resource_types::rsrc_q_t* q_name =
      rp->lookup_name("test.ag", "k_used", uvm_resource<int>::get_type(), false);
    uvm_resource_types::rsrc_q_t* q_type =
      rp->lookup_type("test.ag", uvm_resource<int>::get_type());
    uvm_resource_types::rsrc_q_t* q_re_names_plain =
      rp->lookup_regex_names("test.ag", "k_used", uvm_resource<int>::get_type());
    uvm_resource_types::rsrc_q_t* q_re =
      rp->lookup_regex("/^k_.*$/", "test.ag");
    uvm_resource_types::rsrc_q_t* q_scope =
      rp->lookup_scope("test.ag");
    uvm_resource_types::rsrc_q_t* q_unused =
      rp->find_unused_resources();

    // Also add a wildcard name resource to exercise m_has_wildcard_names == true
    uvm_config_db<int>::set(this, "ag", "w_*", 77);
    uvm_resource_types::rsrc_q_t* q_re_names_wild =
      rp->lookup_regex_names("test.ag", "w_item", uvm_resource<int>::get_type());

    bool func_ok = (q_name && q_name->size() == 1) &&
                   (q_type && q_type->size() >= 2) &&
                   (q_re_names_plain && q_re_names_plain->size() == 1) &&
                   (q_re && q_re->size() == 2) &&
                   (q_scope && q_scope->size() >= 3) &&
                   (q_unused && q_unused->size() >= 1) &&
                   (q_re_names_wild && q_re_names_wild->size() == 1);

    // uvm_agent check
    func_ok = func_ok && (ag->get_is_active() == UVM_PASSIVE);

    // uvm_reg_block::find_blocks check
    std::vector<uvm_reg_block*> found_blks;
    int n_blks = uvm_reg_block::find_blocks("leak_blk", found_blks);
    func_ok = func_ok && (n_blks == 1 && found_blks.size() == 1 && found_blks[0] == blk);

    // uvm_component::print_config check (suppress output noise during test)
    ag->set_report_id_action("CFGPRT", UVM_NO_ACTION);
    top->set_report_id_action("UVM/RESOURCE/PRINT", UVM_NO_ACTION);
    ag->print_config(false, false);

    // Build a small queue for sort_by_precedence loop
    uvm_resource_types::rsrc_q_t sort_q;
    for (int i = 0; i < q_type->size(); ++i)
      sort_q.push_back(q_type->get(i));

    // Turn off auditing during leak loop so get_record does not allocate
    bool prev_audit = uvm_resource_options::is_auditing();
    uvm_resource_options::turn_off_auditing();

    // Warm up
    int v = 0;
    for (int i = 0; i < 100; ++i)
    {
      rp->get_by_name("test.ag", "k_used", uvm_resource<int>::get_type(), false);
      rp->get_by_type("test.ag", uvm_resource<int>::get_type());
      uvm_resource_pool::sort_by_precedence(&sort_q);
      uvm_config_db<int>::get(this, "ag", "k_used", v);
      uvm_config_db<int>::exists(this, "ag", "k_used", false);
      rp->print_resources(nullptr);
    }

#ifdef UVM_TEST_CHECK_MALLOC_ZONE
    malloc_statistics_t stats_before;
    malloc_zone_statistics(nullptr, &stats_before);
#endif

    const int N = 10000;
    for (int i = 0; i < N; ++i)
    {
      uvm_resource_base* rb1 =
        rp->get_by_name("test.ag", "k_used", uvm_resource<int>::get_type(), false);
      uvm_resource_base* rb2 =
        rp->get_by_type("test.ag", uvm_resource<int>::get_type());
      uvm_resource_pool::sort_by_precedence(&sort_q);
      bool got = uvm_config_db<int>::get(this, "ag", "k_used", v);
      bool ex = uvm_config_db<int>::exists(this, "ag", "k_used", false);
      rp->print_resources(nullptr);
      if (!rb1 || !rb2 || !got || !ex || v != 42)
        func_ok = false;
    }

    // Exercise wait_modified() loop
    sc_core::sc_process_handle h_wait =
      sc_core::sc_spawn(sc_bind(&test::wait_loop_proc, this));
    for (int i = 0; i < 200; ++i)
    {
      sc_core::wait(sc_core::SC_ZERO_TIME);
      uvm_config_db<int>::set(this, "ag", "dyn_cfg", i);
      sc_core::wait(1, sc_core::SC_NS);
    }
    if (!h_wait.terminated())
      sc_core::wait(h_wait.terminated_event());

#ifdef UVM_TEST_CHECK_MALLOC_ZONE
    malloc_statistics_t stats_after;
    malloc_zone_statistics(nullptr, &stats_after);
    long long block_delta =
      static_cast<long long>(stats_after.blocks_in_use) -
      static_cast<long long>(stats_before.blocks_in_use);
    bool heap_ok = (block_delta < 500);
    if (!heap_ok)
    {
      std::cout << "HEAP LEAK DETECTED: blocks_in_use delta=" << block_delta << std::endl;
      UVM_ERROR("LEAK", "excessive live heap growth in resource pool loop");
    }
#else
    bool heap_ok = true;
#endif

    // sort_by_precedence(nullptr) safety check (after leak check so leak is also reported pre-fix)
    uvm_resource_pool::sort_by_precedence(nullptr);

    if (prev_audit)
      uvm_resource_options::turn_on_auditing();
    ag->set_report_id_action("CFGPRT", UVM_DISPLAY);
    top->set_report_id_action("UVM/RESOURCE/PRINT", UVM_DISPLAY);

    std::cout << "func_ok=" << func_ok << " heap_ok=" << heap_ok << std::endl;
    if (func_ok && heap_ok)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "!! UVM TEST FAILED !!" << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
