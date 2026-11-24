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

// Reproduces and verifies Fix 3 (pull uvm_sequencer mid-grant kill recovery):
// - get_next_item(): mid-grant seq->kill() and direct sc_process_handle::kill()
// - get(): mid-grant seq->kill()
// - peek(): mid-grant seq->kill()
// - try_next_item(): mid-grant seq->kill() and flag recovery without wedging
// - Restarting a mid-grant killed sequence instance afterwards
// - Attached parent sequence kill() mid-grant with 2 active child sequences
// - Virtual parent sequence kill() mid-grant with 1 active child sequence

#include <systemc>
#include <uvm>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace uvm;

class item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(item);

  int value;

  item( const std::string& name = "item" ) : uvm_sequence_item(name), value(0) {}
};

enum drv_mode_t
{
  MODE_IDLE = 0,
  MODE_GET_NEXT_ITEM = 1,
  MODE_GET = 2,
  MODE_PEEK = 3,
  MODE_TRY_NEXT_ITEM = 4
};

class pull_driver : public uvm_driver<item>
{
 public:
  UVM_COMPONENT_UTILS(pull_driver);

  drv_mode_t mode;
  int remaining;
  sc_core::sc_event start_mode_ev;
  sc_core::sc_event idle_ev;
  sc_core::sc_event try_poll_ev;

  pull_driver( uvm_component_name name )
  : uvm_driver<item>(name), mode(MODE_IDLE), remaining(0) {}

  void start_mode( drv_mode_t m, int count )
  {
    mode = m;
    remaining = count;
    start_mode_ev.notify();
  }

  void wait_idle()
  {
    while (remaining > 0)
      sc_core::wait(idle_ev);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    item req;

    while (true)
    {
      while (mode == MODE_IDLE || remaining <= 0)
      {
        idle_ev.notify();
        sc_core::wait(start_mode_ev);
      }

      if (mode == MODE_GET_NEXT_ITEM)
      {
        seq_item_port->get_next_item(req);
        sc_core::wait(10, sc_core::SC_NS);
        std::cout << sc_core::sc_time_stamp() << ": driver(get_next_item) got item " << req.value << std::endl;
        seq_item_port->item_done();
        remaining--;
      }
      else if (mode == MODE_GET)
      {
        seq_item_port->get(req);
        sc_core::wait(10, sc_core::SC_NS);
        std::cout << sc_core::sc_time_stamp() << ": driver(get) got item " << req.value << std::endl;
        remaining--;
      }
      else if (mode == MODE_PEEK)
      {
        seq_item_port->peek(req);
        sc_core::wait(10, sc_core::SC_NS);
        std::cout << sc_core::sc_time_stamp() << ": driver(peek) got item " << req.value << std::endl;
        seq_item_port->item_done();
        remaining--;
      }
      else
      {
        if (seq_item_port->try_next_item(req))
        {
          sc_core::wait(10, sc_core::SC_NS);
          std::cout << sc_core::sc_time_stamp() << ": driver(try_next_item) got item " << req.value << std::endl;
          seq_item_port->item_done();
          remaining--;
        }
        else
        {
          sc_core::wait(try_poll_ev);
        }
      }
    }
  }
};

class victim_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(victim_seq);

  int value;
  int stall_ns;
  bool kill_self_in_predo;
  uvm_sequence_base* kill_target_in_predo;
  sc_core::sc_event* notify_on_predo;

  victim_seq( const std::string& name = "victim_seq" )
  : uvm_sequence<item>(name), value(999), stall_ns(5), kill_self_in_predo(false),
    kill_target_in_predo(nullptr), notify_on_predo(nullptr) {}

  virtual void pre_do( bool is_item )
  {
    if (is_item)
    {
      if (notify_on_predo != nullptr)
        notify_on_predo->notify();
      if (kill_self_in_predo)
      {
        this->kill();
        return;
      }
      if (stall_ns > 0)
        sc_core::wait(stall_ns, sc_core::SC_NS);
      if (kill_target_in_predo != nullptr)
      {
        kill_target_in_predo->kill();
        return;
      }
    }
  }

  virtual void body()
  {
    item* req = item::type_id::create("req");
    req->value = value;
    start_item(req);
    finish_item(req);
    item::type_id::destroy(req);
  }
};

class survivor_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(survivor_seq);

  int first;
  int count;

  survivor_seq( const std::string& name = "survivor_seq" )
  : uvm_sequence<item>(name), first(100), count(2) {}

  virtual void body()
  {
    for (int i = 0; i < count; i++)
    {
      item* req = item::type_id::create("req");
      req->value = first + i;
      start_item(req);
      finish_item(req);
      item::type_id::destroy(req);
    }
  }
};

class parent_kill_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(parent_kill_seq);

  uvm_sequencer<item>* target_sqr;
  int num_children;
  victim_seq* children[2];

  parent_kill_seq( const std::string& name = "parent_kill_seq" )
  : uvm_sequence<item>(name), target_sqr(nullptr), num_children(2)
  {
    children[0] = nullptr;
    children[1] = nullptr;
  }

  virtual void body()
  {
    std::vector<sc_core::sc_process_handle> procs;
    for (int i = 0; i < num_children; i++)
    {
      children[i] = victim_seq::type_id::create(i == 0 ? "child0" : "child1");
      children[i]->value = 900 + i;
      children[i]->stall_ns = 5;
      victim_seq* c = children[i];
      procs.push_back(sc_core::sc_spawn([this, c]() {
        c->start(target_sqr, this);
      }));
    }
    for (size_t i = 0; i < procs.size(); i++)
    {
      if (procs[i].valid() && !procs[i].terminated())
        sc_core::wait(procs[i].terminated_event());
    }
  }
};

class child_kill_body_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(child_kill_body_seq);

  uvm_sequence_base* target_to_kill;
  int wait_ns;

  child_kill_body_seq( const std::string& name = "child_kill_body_seq" )
  : uvm_sequence<item>(name), target_to_kill(nullptr), wait_ns(2) {}

  virtual void body()
  {
    if (wait_ns > 0)
      sc_core::wait(wait_ns, sc_core::SC_NS);
    if (target_to_kill != nullptr)
      target_to_kill->kill();
  }
};

class child_kills_parent_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(child_kills_parent_seq);

  uvm_sequencer<item>* target_sqr;
  bool kill_from_predo;
  uvm_sequence_base* kill_target;
  bool ran_after_join;
  uvm_sequence_base* c0;
  victim_seq* c1;

  child_kills_parent_seq( const std::string& name = "child_kills_parent_seq" )
  : uvm_sequence<item>(name), target_sqr(nullptr), kill_from_predo(false),
    kill_target(nullptr), ran_after_join(false), c0(nullptr), c1(nullptr) {}

  virtual void body()
  {
    uvm_sequence_base* tgt = (kill_target != nullptr) ? kill_target : this;
    c1 = victim_seq::type_id::create("c1");
    c1->value = 951;
    c1->stall_ns = 200;

    std::vector<sc_core::sc_process_handle> procs;
    if (kill_from_predo)
    {
      victim_seq* v0 = victim_seq::type_id::create("c0");
      v0->value = 950;
      v0->stall_ns = 2;
      v0->kill_target_in_predo = tgt;
      c0 = v0;
      procs.push_back(sc_core::sc_spawn([this, v0]() { v0->start(target_sqr, this); }));
      procs.push_back(sc_core::sc_spawn([this]() {
        sc_core::wait(1, sc_core::SC_NS);
        c1->start(target_sqr, this);
      }));
    }
    else
    {
      child_kill_body_seq* b0 = child_kill_body_seq::type_id::create("c0");
      b0->target_to_kill = tgt;
      b0->wait_ns = 3;
      c0 = b0;
      procs.push_back(sc_core::sc_spawn([this]() { c1->start(target_sqr, this); }));
      procs.push_back(sc_core::sc_spawn([this, b0]() { b0->start(target_sqr, this); }));
    }

    for (size_t i = 0; i < procs.size(); i++)
    {
      if (procs[i].valid() && !procs[i].terminated())
        sc_core::wait(procs[i].terminated_event());
    }
    ran_after_join = true;
  }
};

class grandparent_kill_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(grandparent_kill_seq);

  uvm_sequencer<item>* target_sqr;
  child_kills_parent_seq* child_par;
  bool ran_after_join;

  grandparent_kill_seq( const std::string& name = "grandparent_kill_seq" )
  : uvm_sequence<item>(name), target_sqr(nullptr), child_par(nullptr), ran_after_join(false) {}

  virtual void body()
  {
    child_par = child_kills_parent_seq::type_id::create("mid_par");
    child_par->target_sqr = target_sqr;
    child_par->kill_from_predo = false;
    child_par->kill_target = this;
    child_par->start(target_sqr, this);
    ran_after_join = true;
  }
};

class stop_caller_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(stop_caller_seq);

  sc_core::sc_event trigger_ev;

  stop_caller_seq( const std::string& name = "stop_caller_seq" )
  : uvm_sequence<item>(name) {}

  virtual void body()
  {
    sc_core::wait(trigger_ev);
    m_sequencer->stop_sequences();
  }
};

class irrel_wait_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(irrel_wait_seq);

  irrel_wait_seq( const std::string& name = "irrel_wait_seq" )
  : uvm_sequence<item>(name) {}

  virtual bool is_relevant() const { return false; }

  virtual void wait_for_relevant() const
  {
    sc_core::sc_event never;
    sc_core::wait(never);
  }

  virtual void body()
  {
    item* req = item::type_id::create("req");
    start_item(req);
    finish_item(req);
    item::type_id::destroy(req);
  }
};

class auto_obj_selfkill_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(auto_obj_selfkill_seq);

  auto_obj_selfkill_seq( const std::string& name = "auto_obj_selfkill_seq" )
  : uvm_sequence<item>(name) {}

  virtual void body()
  {
    sc_core::wait(1, sc_core::SC_NS);
    this->kill();
  }
};

class local_push_driver : public uvm_push_driver<item>
{
 public:
  UVM_COMPONENT_UTILS(local_push_driver);

  std::vector<int> received_values;

  local_push_driver( uvm_component_name name ) : uvm_push_driver<item>(name) {}

  virtual void put( const item& t )
  {
    sc_core::wait(5, sc_core::SC_NS);
    received_values.push_back(t.value);
    std::cout << sc_core::sc_time_stamp() << ": push_driver got item " << t.value << std::endl;
  }
};

class send_and_stop_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(send_and_stop_seq);

  int item_val;

  send_and_stop_seq( const std::string& name = "send_and_stop_seq" )
  : uvm_sequence<item>(name), item_val(8881) {}

  virtual void body()
  {
    item* req = item::type_id::create("stale_req");
    req->value = item_val;
    m_sequencer->wait_for_grant(this);
    m_sequencer->send_request(this, req);
    item::type_id::destroy(req);
    m_sequencer->stop_sequences();
  }
};

class immediate_stop_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(immediate_stop_seq);

  immediate_stop_seq( const std::string& name = "immediate_stop_seq" )
  : uvm_sequence<item>(name) {}

  virtual void body()
  {
    m_sequencer->stop_sequences();
  }
};

class dokill_reentrant_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(dokill_reentrant_seq);

  int do_kill_calls;
  uvm_sequence_base* peer;

  dokill_reentrant_seq( const std::string& name = "dokill_reentrant_seq" )
  : uvm_sequence<item>(name), do_kill_calls(0), peer(nullptr) {}

  virtual void do_kill()
  {
    do_kill_calls++;
    if (peer != nullptr)
      peer->kill();
    else
      this->kill();
  }

  virtual void body()
  {
    sc_core::sc_event never;
    sc_core::wait(never);
  }
};

class scfork_parent_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(scfork_parent_seq);

  uvm_sequencer<item>* target_sqr;
  bool call_stop_sequences;
  bool ran_after_join;
  uvm_sequence_base* c0;
  victim_seq* c1;

  scfork_parent_seq( const std::string& name = "scfork_parent_seq" )
  : uvm_sequence<item>(name), target_sqr(nullptr), call_stop_sequences(false),
    ran_after_join(false), c0(nullptr), c1(nullptr) {}

  struct ScJoinStackClobber
  {
    char buf[256];
    ScJoinStackClobber() { std::memset(buf, 0x55, sizeof(buf)); }
    ~ScJoinStackClobber() { std::memset(buf, 0xAA, sizeof(buf)); }
  };

  virtual void body()
  {
    ScJoinStackClobber clobber;
    c1 = victim_seq::type_id::create("scfork_c1");
    c1->value = 1491;
    c1->stall_ns = 200;

    if (call_stop_sequences)
    {
      stop_caller_seq* s0 = stop_caller_seq::type_id::create("scfork_s0");
      c0 = s0;
      SC_FORK
        sc_core::sc_spawn([this]() { c1->start(target_sqr, this); }),
        sc_core::sc_spawn([this, s0]() {
          sc_core::wait(2, sc_core::SC_NS);
          s0->trigger_ev.notify();
        }),
        sc_core::sc_spawn([this, s0]() { s0->start(target_sqr, this); })
      SC_JOIN
    }
    else
    {
      child_kill_body_seq* b0 = child_kill_body_seq::type_id::create("scfork_c0");
      b0->target_to_kill = this;
      b0->wait_ns = 2;
      c0 = b0;
      SC_FORK
        sc_core::sc_spawn([this]() { c1->start(target_sqr, this); }),
        sc_core::sc_spawn([this, b0]() { b0->start(target_sqr, this); })
      SC_JOIN
    }
    ran_after_join = (clobber.buf[0] != 0);
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_sequencer<item>* sqr;
  pull_driver* drv;
  uvm_push_sequencer<item>* psqr;
  local_push_driver* pdrv;

  test( uvm_component_name name ) : uvm_test(name), sqr(nullptr), drv(nullptr), psqr(nullptr), pdrv(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    sqr = new uvm_sequencer<item>("sqr");
    drv = pull_driver::type_id::create("drv", this);
    psqr = new uvm_push_sequencer<item>("psqr");
    pdrv = local_push_driver::type_id::create("pdrv", this);
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    drv->seq_item_port.connect(sqr->seq_item_export);
    psqr->req_port.connect(pdrv->req_export);
  }

  void run_kill_with_survivor( drv_mode_t mode, int first_val )
  {
    drv->start_mode(mode, 2);

    victim_seq* victim = victim_seq::type_id::create("victim");
    survivor_seq* surv = survivor_seq::type_id::create("surv");
    surv->first = first_val;
    surv->count = 2;

    sc_core::sc_process_handle hv = sc_core::sc_spawn([&]() { victim->start(sqr); });
    sc_core::wait(1, sc_core::SC_NS);
    sc_core::sc_process_handle hs = sc_core::sc_spawn([&]() { surv->start(sqr); });
    sc_core::wait(1, sc_core::SC_NS);

    victim->kill();
    if (hv.valid() && !hv.terminated())
      sc_core::wait(hv.terminated_event());
    sc_core::wait(hs.terminated_event());
    drv->wait_idle();
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // 1. get_next_item mid-grant seq->kill()
    run_kill_with_survivor(MODE_GET_NEXT_ITEM, 100);

    // 2. get_next_item mid-grant direct sc_process_handle::kill() (zombie grant)
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 2);
      victim_seq* zseq = victim_seq::type_id::create("zseq");
      survivor_seq* surv = survivor_seq::type_id::create("surv_z");
      surv->first = 200;
      surv->count = 2;

      sc_core::sc_process_handle hz = sc_core::sc_spawn([&]() {
        sqr->wait_for_grant(zseq);
        sc_core::wait(5, sc_core::SC_NS);
      });
      sc_core::wait(1, sc_core::SC_NS);
      sc_core::sc_process_handle hs = sc_core::sc_spawn([&]() { surv->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);
      hz.kill();
      sc_core::wait(hs.terminated_event());
      drv->wait_idle();
    }

    // 3. get() mid-grant seq->kill()
    run_kill_with_survivor(MODE_GET, 300);

    // 4. peek() mid-grant seq->kill()
    run_kill_with_survivor(MODE_PEEK, 400);

    // 5. try_next_item(): mid-grant kill + subsequent try_next_item() must not wedge
    {
      drv->start_mode(MODE_TRY_NEXT_ITEM, 1);
      victim_seq* victim = victim_seq::type_id::create("try_victim");
      victim->kill_self_in_predo = true;
      victim->stall_ns = 0;

      survivor_seq* surv = survivor_seq::type_id::create("try_surv");
      surv->first = 500;
      surv->count = 1;

      sc_core::sc_process_handle hv = sc_core::sc_spawn([&]() { victim->start(sqr); });
      sc_core::sc_process_handle hs = sc_core::sc_spawn([&]() { surv->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);
      drv->try_poll_ev.notify();
      sc_core::wait(hs.terminated_event());
      if (hv.valid() && !hv.terminated())
        sc_core::wait(hv.terminated_event());
      drv->wait_idle();
    }

    // 6. Restart a previously mid-grant killed sequence instance
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 1);
      sc_core::wait(1, sc_core::SC_NS);

      victim_seq* reused = victim_seq::type_id::create("reused");
      reused->value = 600;
      reused->stall_ns = 5;
      sc_core::sc_process_handle hr = sc_core::sc_spawn([&]() { reused->start(sqr); });
      sc_core::wait(2, sc_core::SC_NS);
      reused->kill();
      if (hr.valid() && !hr.terminated())
        sc_core::wait(hr.terminated_event());

      reused->value = 601;
      reused->stall_ns = 0;
      reused->start(sqr);
    }

    // 7. Attached parent kill() mid-grant with 2 active child sequences
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 2);
      parent_kill_seq* p = parent_kill_seq::type_id::create("parent_attached");
      p->target_sqr = sqr;
      p->num_children = 2;

      survivor_seq* surv = survivor_seq::type_id::create("surv_parent");
      surv->first = 700;
      surv->count = 2;

      sc_core::sc_process_handle hp = sc_core::sc_spawn([&]() { p->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);
      sc_core::sc_process_handle hs = sc_core::sc_spawn([&]() { surv->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);

      p->kill();
      if (hp.valid() && !hp.terminated())
        sc_core::wait(hp.terminated_event());
      sc_core::wait(hs.terminated_event());
      drv->wait_idle();
      std::cout << sc_core::sc_time_stamp() << ": attached parent kill term=" << hp.terminated()
                << " state=" << p->get_sequence_state()
                << " c0=" << p->children[0]->get_sequence_state()
                << " c1=" << p->children[1]->get_sequence_state() << std::endl;
    }

    // 8. Virtual parent kill() mid-grant with 1 active child sequence
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 1);
      parent_kill_seq* vp = parent_kill_seq::type_id::create("parent_virtual");
      vp->target_sqr = sqr;
      vp->num_children = 1;

      survivor_seq* surv = survivor_seq::type_id::create("surv_vparent");
      surv->first = 800;
      surv->count = 1;

      sc_core::sc_process_handle hvp = sc_core::sc_spawn([&]() { vp->start(nullptr); });
      sc_core::wait(1, sc_core::SC_NS);
      sc_core::sc_process_handle hs = sc_core::sc_spawn([&]() { surv->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);

      vp->kill();
      if (hvp.valid() && !hvp.terminated())
        sc_core::wait(hvp.terminated_event());
      sc_core::wait(hs.terminated_event());
      drv->wait_idle();
      std::cout << sc_core::sc_time_stamp() << ": virtual parent kill term=" << hvp.terminated()
                << " state=" << vp->get_sequence_state()
                << " c0=" << vp->children[0]->get_sequence_state() << std::endl;
    }

    // 9a. Child kills attached parent from pre_do() while sibling is active
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 1);
      child_kills_parent_seq* p = child_kills_parent_seq::type_id::create("ckp_attached");
      p->target_sqr = sqr;
      p->kill_from_predo = true;

      survivor_seq* surv = survivor_seq::type_id::create("surv_ckp_a");
      surv->first = 900;
      surv->count = 1;

      sc_core::sc_process_handle hp = sc_core::sc_spawn([&]() { p->start(sqr); });
      sc_core::wait(10, sc_core::SC_NS);
      if (hp.valid() && !hp.terminated())
        sc_core::wait(20, sc_core::SC_NS, hp.terminated_event());
      bool term_a = hp.terminated();
      uvm_sequence_state_enum st_a = p->get_sequence_state();
      uvm_sequence_state_enum c0_a = p->c0->get_sequence_state();
      uvm_sequence_state_enum c1_a = p->c1->get_sequence_state();
      bool aj_a = p->ran_after_join;
      if (!term_a) { p->c1->kill(); p->kill(); sc_core::wait(1, sc_core::SC_NS); }

      surv->start(sqr);
      drv->wait_idle();
      std::cout << sc_core::sc_time_stamp() << ": child-kills-parent(predo) term=" << term_a
                << " state=" << st_a
                << " c0=" << c0_a
                << " c1=" << c1_a
                << " after_join=" << aj_a << std::endl;
    }

    // 9b. Child kills virtual parent from body() while sibling is stalled mid-grant
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 1);
      child_kills_parent_seq* vp = child_kills_parent_seq::type_id::create("ckp_virtual");
      vp->target_sqr = sqr;
      vp->kill_from_predo = false;

      survivor_seq* surv = survivor_seq::type_id::create("surv_ckp_v");
      surv->first = 910;
      surv->count = 1;

      sc_core::sc_process_handle hvp = sc_core::sc_spawn([&]() { vp->start(nullptr); });
      sc_core::wait(10, sc_core::SC_NS);
      if (hvp.valid() && !hvp.terminated())
        sc_core::wait(20, sc_core::SC_NS, hvp.terminated_event());
      bool term_b = hvp.terminated();
      uvm_sequence_state_enum st_b = vp->get_sequence_state();
      uvm_sequence_state_enum c0_b = vp->c0->get_sequence_state();
      uvm_sequence_state_enum c1_b = vp->c1->get_sequence_state();
      bool aj_b = vp->ran_after_join;
      if (!term_b) { vp->c1->kill(); vp->kill(); sc_core::wait(1, sc_core::SC_NS); }

      surv->start(sqr);
      drv->wait_idle();
      std::cout << sc_core::sc_time_stamp() << ": child-kills-parent(body) term=" << term_b
                << " state=" << st_b
                << " c0=" << c0_b
                << " c1=" << c1_b
                << " after_join=" << aj_b << std::endl;
    }

    // 9c. 3-level grandchild kills grandparent from body()
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 1);
      grandparent_kill_seq* gp = grandparent_kill_seq::type_id::create("ckp_grandparent");
      gp->target_sqr = sqr;

      survivor_seq* surv = survivor_seq::type_id::create("surv_ckp_gp");
      surv->first = 920;
      surv->count = 1;

      sc_core::sc_process_handle hgp = sc_core::sc_spawn([&]() { gp->start(sqr); });
      sc_core::wait(10, sc_core::SC_NS);
      if (hgp.valid() && !hgp.terminated())
        sc_core::wait(20, sc_core::SC_NS, hgp.terminated_event());
      bool term_c = hgp.terminated();
      uvm_sequence_state_enum gp_st = gp->get_sequence_state();
      uvm_sequence_state_enum p_st = gp->child_par->get_sequence_state();
      uvm_sequence_state_enum c0_c = gp->child_par->c0->get_sequence_state();
      uvm_sequence_state_enum c1_c = gp->child_par->c1->get_sequence_state();
      bool aj_c = gp->ran_after_join;
      if (!term_c) { gp->child_par->c1->kill(); gp->kill(); sc_core::wait(1, sc_core::SC_NS); }

      surv->start(sqr);
      drv->wait_idle();
      std::cout << sc_core::sc_time_stamp() << ": grandchild-kills-grandparent term=" << term_c
                << " gp=" << gp_st
                << " p=" << p_st
                << " c0=" << c0_c
                << " c1=" << c1_c
                << " after_join=" << aj_c << std::endl;
    }

    // 10. Lowest-seq_id active sequence calls sqr->stop_sequences() from body()
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 1);
      stop_caller_seq* s0 = stop_caller_seq::type_id::create("s0_lowest_id");
      victim_seq* s1 = victim_seq::type_id::create("s1_mid_grant");
      s1->value = 991;
      s1->stall_ns = 200;
      irrel_wait_seq* s2 = irrel_wait_seq::type_id::create("s2_irrel");

      sc_core::sc_process_handle h0 = sc_core::sc_spawn([&]() { s0->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);
      sc_core::sc_process_handle h1 = sc_core::sc_spawn([&]() { s1->start(sqr); });
      sc_core::sc_process_handle h2 = sc_core::sc_spawn([&]() { s2->start(sqr); });
      sc_core::wait(2, sc_core::SC_NS);

      s0->trigger_ev.notify();
      sc_core::wait(5, sc_core::SC_NS);
      if (h0.valid() && !h0.terminated())
        sc_core::wait(10, sc_core::SC_NS, h0.terminated_event());
      if (h1.valid() && !h1.terminated())
        sc_core::wait(10, sc_core::SC_NS, h1.terminated_event());
      if (h2.valid() && !h2.terminated())
        sc_core::wait(10, sc_core::SC_NS, h2.terminated_event());

      bool t0 = h0.terminated(), t1 = h1.terminated(), t2 = h2.terminated();
      uvm_sequence_state_enum st0 = s0->get_sequence_state();
      uvm_sequence_state_enum st1 = s1->get_sequence_state();
      uvm_sequence_state_enum st2 = s2->get_sequence_state();
      if (!t1) s1->kill();
      if (!t2) s2->kill();
      if (!t1 || !t2) sc_core::wait(1, sc_core::SC_NS);

      survivor_seq* surv = survivor_seq::type_id::create("surv_stop_order");
      surv->first = 1000;
      surv->count = 1;
      surv->start(sqr);
      drv->wait_idle();
      std::cout << sc_core::sc_time_stamp() << ": lowest-id stop_sequences terms="
                << t0 << "," << t1 << "," << t2
                << " states=" << st0 << "," << st1 << "," << st2 << std::endl;
    }

    // 11. Self-kill with automatic_phase_objection drops starting phase objection
    {
      auto_obj_selfkill_seq* sa = auto_obj_selfkill_seq::type_id::create("sa_auto");
      sa->set_starting_phase(&phase);
      sa->set_automatic_phase_objection(true);
      sc_core::sc_process_handle ha = sc_core::sc_spawn([&]() { sa->start(sqr); });
      sc_core::wait(ha.terminated_event());
      int obj_a = phase.get_objection()->get_objection_count(sa);
      if (obj_a > 0)
        phase.drop_objection(sa, "cleanup leaked objection", obj_a);

      auto_obj_selfkill_seq* sv = auto_obj_selfkill_seq::type_id::create("sv_auto");
      sv->set_starting_phase(&phase);
      sv->set_automatic_phase_objection(true);
      sc_core::sc_process_handle hv = sc_core::sc_spawn([&]() { sv->start(nullptr); });
      sc_core::wait(hv.terminated_event());
      int obj_v = phase.get_objection()->get_objection_count(sv);
      if (obj_v > 0)
        phase.drop_objection(sv, "cleanup leaked objection", obj_v);

      std::cout << sc_core::sc_time_stamp() << ": auto_obj self-kill leaked_counts="
                << obj_a << "," << obj_v
                << " states=" << sa->get_sequence_state()
                << "," << sv->get_sequence_state() << std::endl;
    }

    // 12a. uvm_sequencer::stop_sequences() self-stop drains m_req_fifo and resets flags
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 1);
      send_and_stop_seq* s_stop = send_and_stop_seq::type_id::create("s_pull_stop");
      s_stop->item_val = 8881;
      sc_core::sc_process_handle hs = sc_core::sc_spawn([&]() { s_stop->start(sqr); });
      sc_core::wait(hs.terminated_event());

      const int fifo_used_after_stop = sqr->m_req_fifo.used();
      const bool req_flag_after_stop = sqr->sequence_item_requested;
      const bool gni_flag_after_stop = sqr->get_next_item_called;

      // Also verify flag reset when driver is idle and sequence calls stop_sequences()
      const bool saved_req = sqr->sequence_item_requested;
      const bool saved_gni = sqr->get_next_item_called;
      sqr->sequence_item_requested = true;
      sqr->get_next_item_called = true;
      immediate_stop_seq* s_imm = immediate_stop_seq::type_id::create("s_pull_imm_stop");
      sc_core::sc_process_handle hi = sc_core::sc_spawn([&]() { s_imm->start(sqr); });
      sc_core::wait(hi.terminated_event());

      const bool req_flag_after_imm = sqr->sequence_item_requested;
      const bool gni_flag_after_imm = sqr->get_next_item_called;

      if (saved_req || saved_gni)
      {
        // Before fix, drv may have already consumed 8881 and gone idle; restore cleanly for remaining tests
      }

      survivor_seq* surv = survivor_seq::type_id::create("surv_pull_self_stop");
      surv->first = 1200;
      surv->count = 1;
      sc_core::sc_process_handle hsurv = sc_core::sc_spawn([&]() { surv->start(sqr); });
      sc_core::wait(25, sc_core::SC_NS, hsurv.terminated_event());
      const bool surv_done = hsurv.terminated();
      if (!surv_done)
        surv->kill();
      else
        drv->wait_idle();

      std::cout << sc_core::sc_time_stamp() << ": pull self-stop fifo_used=" << fifo_used_after_stop
                << " flags1=" << req_flag_after_stop << "," << gni_flag_after_stop
                << " flags2=" << req_flag_after_imm << "," << gni_flag_after_imm
                << " surv_done=" << surv_done << std::endl;
    }

    // 12b. uvm_push_sequencer::stop_sequences() self-stop drains m_req_fifo before unwind
    {
      send_and_stop_seq* ps_stop = send_and_stop_seq::type_id::create("s_push_stop");
      ps_stop->item_val = 8882;
      sc_core::sc_process_handle hp = sc_core::sc_spawn([&]() { ps_stop->start(psqr); });
      sc_core::wait(hp.terminated_event());
      const int push_fifo_used = psqr->m_req_fifo.used();

      survivor_seq* psurv = survivor_seq::type_id::create("surv_push_self_stop");
      psurv->first = 1201;
      psurv->count = 1;
      psurv->start(psqr);
      sc_core::wait(10, sc_core::SC_NS);

      std::cout << sc_core::sc_time_stamp() << ": push self-stop fifo_used=" << push_fifo_used
                << " recv_count=" << pdrv->received_values.size()
                << " first_recv=" << (pdrv->received_values.empty() ? -1 : pdrv->received_values[0])
                << std::endl;
    }

    // 13. do_kill() reentrancy (attached, virtual, mutual) must not stack overflow
    {
      dokill_reentrant_seq* da = dokill_reentrant_seq::type_id::create("da_reent");
      sc_core::sc_process_handle hda = sc_core::sc_spawn([&]() { da->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);
      da->kill();
      if (hda.valid() && !hda.terminated())
        sc_core::wait(hda.terminated_event());

      dokill_reentrant_seq* dv = dokill_reentrant_seq::type_id::create("dv_reent");
      sc_core::sc_process_handle hdv = sc_core::sc_spawn([&]() { dv->start(nullptr); });
      sc_core::wait(1, sc_core::SC_NS);
      dv->kill();
      if (hdv.valid() && !hdv.terminated())
        sc_core::wait(hdv.terminated_event());

      dokill_reentrant_seq* ma = dokill_reentrant_seq::type_id::create("ma_mutual");
      dokill_reentrant_seq* mb = dokill_reentrant_seq::type_id::create("mb_mutual");
      ma->peer = mb;
      mb->peer = ma;
      sc_core::sc_process_handle hma = sc_core::sc_spawn([&]() { ma->start(sqr); });
      sc_core::sc_process_handle hmb = sc_core::sc_spawn([&]() { mb->start(nullptr); });
      sc_core::wait(1, sc_core::SC_NS);
      ma->kill();
      if (hma.valid() && !hma.terminated())
        sc_core::wait(hma.terminated_event());
      if (hmb.valid() && !hmb.terminated())
        sc_core::wait(hmb.terminated_event());

      std::cout << sc_core::sc_time_stamp() << ": do_kill reentrancy states="
                << da->get_sequence_state() << "," << dv->get_sequence_state() << ","
                << ma->get_sequence_state() << "," << mb->get_sequence_state()
                << " calls=" << da->do_kill_calls << "," << dv->do_kill_calls << ","
                << ma->do_kill_calls << "," << mb->do_kill_calls << std::endl;
    }

    // 14. SC_FORK ... SC_JOIN wrapping child->start(sqr, this) + local RAII stack clobber
    {
      drv->start_mode(MODE_GET_NEXT_ITEM, 1);
      scfork_parent_seq* sp1 = scfork_parent_seq::type_id::create("scfork_p1");
      sp1->target_sqr = sqr;
      sp1->call_stop_sequences = false;
      sc_core::sc_process_handle h1 = sc_core::sc_spawn([&]() { sp1->start(sqr); });
      if (h1.valid() && !h1.terminated())
        sc_core::wait(h1.terminated_event());
      survivor_seq* surv1 = survivor_seq::type_id::create("surv_scfork1");
      surv1->first = 1400;
      surv1->count = 1;
      surv1->start(sqr);
      drv->wait_idle();
      std::cout << sc_core::sc_time_stamp() << ": scfork child-kills-parent term=" << h1.terminated()
                << " state=" << sp1->get_sequence_state()
                << " c0=" << sp1->c0->get_sequence_state()
                << " c1=" << sp1->c1->get_sequence_state()
                << " after_join=" << sp1->ran_after_join << std::endl;

      drv->start_mode(MODE_GET_NEXT_ITEM, 1);
      scfork_parent_seq* sp2 = scfork_parent_seq::type_id::create("scfork_p2");
      sp2->target_sqr = sqr;
      sp2->call_stop_sequences = true;
      sc_core::sc_process_handle h2 = sc_core::sc_spawn([&]() { sp2->start(sqr); });
      if (h2.valid() && !h2.terminated())
        sc_core::wait(h2.terminated_event());
      survivor_seq* surv2 = survivor_seq::type_id::create("surv_scfork2");
      surv2->first = 1410;
      surv2->count = 1;
      surv2->start(sqr);
      drv->wait_idle();
      std::cout << sc_core::sc_time_stamp() << ": scfork child-stop-sequences term=" << h2.terminated()
                << " state=" << sp2->get_sequence_state()
                << " c0=" << sp2->c0->get_sequence_state()
                << " c1=" << sp2->c1->get_sequence_state()
                << " after_join=" << sp2->ran_after_join << std::endl;
    }

    std::cout << "sequencer is " << (sqr->is_grabbed() ? "still locked" : "free") << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  run_test("test");
  return 0;
}
