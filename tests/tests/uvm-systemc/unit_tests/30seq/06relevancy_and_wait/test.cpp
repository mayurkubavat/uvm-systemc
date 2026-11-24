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

// Reproduces and verifies Fix 1: m_wait_for_available_sequence()
// - Scenario A: irrelevant sequence becomes relevant via wait_for_relevant()
// - Scenario B: multiple irrelevant sequences, one wakes; loser watcher killed
// - Scenario B2: 0-time wait_for_relevant() return (no missed wakeup)
// - Scenario C: locked vs irrelevant (wakes on unlock without polling irrelevant)
// - Scenario D: sequence killed while waiting in wait_for_relevant()
//   (both seq->kill() and direct sc_process_handle::kill())
// - Scenario E: self-kill (this->kill()) inside wait_for_relevant(),
//   both returning immediately and suspending via wait()
// - Scenario F: sequencer->stop_sequences() inside wait_for_relevant()
//   with multiple blocked irrelevant sequences

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(item);

  int value;

  item( const std::string& name = "item" ) : uvm_sequence_item(name), value(0) {}
};

class driver : public uvm_driver<item>
{
 public:
  UVM_COMPONENT_UTILS(driver);

  driver( uvm_component_name name ) : uvm_driver<item>(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    item req;

    while (true)
    {
      seq_item_port->get_next_item(req);
      sc_core::wait(10, sc_core::SC_NS);
      std::cout << sc_core::sc_time_stamp() << ": driver got item " << req.value << std::endl;
      seq_item_port->item_done();
    }
  }
};

class rel_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(rel_seq);

  int value;
  bool relevant;
  bool immediate_rel;
  bool forever_wait;
  bool locking;
  bool self_kill_in_wait;
  bool self_kill_then_wait;
  bool stop_sqr_in_wait;
  mutable int wait_calls;
  mutable int active_watchers;
  sc_core::sc_event rel_ev;

  rel_seq( const std::string& name = "rel_seq" )
  : uvm_sequence<item>(name),
    value(0),
    relevant(true),
    immediate_rel(false),
    forever_wait(false),
    locking(false),
    self_kill_in_wait(false),
    self_kill_then_wait(false),
    stop_sqr_in_wait(false),
    wait_calls(0),
    active_watchers(0)
  {}

  virtual bool is_relevant() const
  {
    return relevant;
  }

  virtual void wait_for_relevant() const
  {
    struct scope_guard
    {
      int& cnt;
      explicit scope_guard(int& c) : cnt(c) { cnt++; }
      ~scope_guard() { cnt--; }
    } guard(active_watchers);

    wait_calls++;
    if (stop_sqr_in_wait)
    {
      if (get_sequencer() != nullptr)
        get_sequencer()->stop_sequences();
      return;
    }
    if (self_kill_in_wait)
    {
      const_cast<rel_seq*>(this)->kill();
      if (self_kill_then_wait)
      {
        sc_core::sc_event never;
        sc_core::wait(never);
      }
      return;
    }
    if (immediate_rel)
    {
      const_cast<rel_seq*>(this)->relevant = true;
      return;
    }
    if (forever_wait)
    {
      sc_core::sc_event never;
      sc_core::wait(never);
      return;
    }
    while (!relevant)
      sc_core::wait(rel_ev);
  }

  virtual void body()
  {
    if (locking)
      m_sequencer->lock(this);

    item* req = item::type_id::create("req");
    req->value = value;
    start_item(req);
    finish_item(req);
    item::type_id::destroy(req);

    if (locking)
      m_sequencer->unlock(this);
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_sequencer<item>* sqr;
  driver* drv;

  test( uvm_component_name name ) : uvm_test(name), sqr(nullptr), drv(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    sqr = new uvm_sequencer<item>("sqr");
    drv = driver::type_id::create("drv", this);
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    drv->seq_item_port.connect(sqr->seq_item_export);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // Scenario A: single irrelevant sequence becomes relevant via wait_for_relevant()
    rel_seq* s1 = rel_seq::type_id::create("s1");
    s1->value = 10;
    s1->relevant = false;
    sc_core::sc_process_handle h1 = sc_core::sc_spawn([&]() { s1->start(sqr); });
    sc_core::wait(20, sc_core::SC_NS);
    std::cout << sc_core::sc_time_stamp() << ": s1 active_watchers=" << s1->active_watchers << std::endl;
    s1->relevant = true;
    s1->rel_ev.notify();
    sc_core::wait(h1.terminated_event());
    std::cout << sc_core::sc_time_stamp() << ": A done wait_calls=" << s1->wait_calls
              << " active=" << s1->active_watchers << std::endl;

    // Scenario B: multiple irrelevant sequences; one wakes, other's watcher is cleaned up
    rel_seq* s_hold = rel_seq::type_id::create("s_hold");
    s_hold->value = 15;
    s_hold->locking = true;
    rel_seq* s2 = rel_seq::type_id::create("s2");
    s2->value = 20;
    s2->relevant = false;
    rel_seq* s3 = rel_seq::type_id::create("s3");
    s3->value = 30;
    s3->relevant = false;

    sc_core::sc_process_handle hh = sc_core::sc_spawn([&]() { s_hold->start(sqr); });
    sc_core::wait(1, sc_core::SC_NS);
    sc_core::sc_process_handle h2 = sc_core::sc_spawn([&]() { s2->start(sqr); });
    sc_core::sc_process_handle h3 = sc_core::sc_spawn([&]() { s3->start(sqr); });
    sc_core::wait(hh.terminated_event());
    sc_core::wait(5, sc_core::SC_NS);
    std::cout << sc_core::sc_time_stamp() << ": B waiting s2_active=" << s2->active_watchers
              << " s3_active=" << s3->active_watchers << std::endl;

    s2->relevant = true;
    s2->rel_ev.notify();
    sc_core::wait(h2.terminated_event());
    sc_core::wait(5, sc_core::SC_NS);
    std::cout << sc_core::sc_time_stamp() << ": B after s2 s3_active=" << s3->active_watchers
              << " s3_wait_calls=" << s3->wait_calls << std::endl;

    s3->relevant = true;
    s3->rel_ev.notify();
    sc_core::wait(h3.terminated_event());
    std::cout << sc_core::sc_time_stamp() << ": B after s3 s3_active=" << s3->active_watchers << std::endl;

    // Scenario B2: 0-time wait_for_relevant() return
    rel_seq* s_imm = rel_seq::type_id::create("s_imm");
    s_imm->value = 35;
    s_imm->relevant = false;
    s_imm->immediate_rel = true;
    s_imm->start(sqr);
    std::cout << sc_core::sc_time_stamp() << ": B2 done wait_calls=" << s_imm->wait_calls << std::endl;

    // Scenario C: locked relevant sequence vs irrelevant sequence
    rel_seq* s_lock = rel_seq::type_id::create("s_lock");
    s_lock->value = 38;
    s_lock->locking = true;
    rel_seq* s_rel = rel_seq::type_id::create("s_rel");
    s_rel->value = 40;
    s_rel->relevant = true;
    rel_seq* s_irrel = rel_seq::type_id::create("s_irrel");
    s_irrel->value = 99;
    s_irrel->relevant = false;
    s_irrel->forever_wait = true;

    sc_core::sc_process_handle hl = sc_core::sc_spawn([&]() { s_lock->start(sqr); });
    sc_core::wait(1, sc_core::SC_NS);
    sc_core::sc_process_handle hr = sc_core::sc_spawn([&]() { s_rel->start(sqr); });
    sc_core::sc_process_handle hi = sc_core::sc_spawn([&]() { s_irrel->start(sqr); });
    sc_core::wait(hl.terminated_event() & hr.terminated_event());
    std::cout << sc_core::sc_time_stamp() << ": C s_rel done, s_irrel wait_calls="
              << s_irrel->wait_calls << std::endl;
    sc_core::wait(5, sc_core::SC_NS);
    s_irrel->kill();
    if (hi.valid() && !hi.terminated())
      sc_core::wait(hi.terminated_event());
    std::cout << sc_core::sc_time_stamp() << ": C s_irrel killed active="
              << s_irrel->active_watchers << std::endl;

    // Scenario D: kill while waiting in wait_for_relevant() (seq->kill() and proc.kill())
    rel_seq* s_kill1 = rel_seq::type_id::create("s_kill1");
    s_kill1->value = 50;
    s_kill1->relevant = false;
    s_kill1->forever_wait = true;
    sc_core::sc_process_handle hk1 = sc_core::sc_spawn([&]() { s_kill1->start(sqr); });
    sc_core::wait(5, sc_core::SC_NS);
    s_kill1->kill();
    if (hk1.valid() && !hk1.terminated())
      sc_core::wait(hk1.terminated_event());
    std::cout << sc_core::sc_time_stamp() << ": D1 after seq->kill active="
              << s_kill1->active_watchers << std::endl;

    rel_seq* s_kill2 = rel_seq::type_id::create("s_kill2");
    s_kill2->value = 60;
    s_kill2->relevant = false;
    s_kill2->forever_wait = true;
    sc_core::sc_process_handle hk2 = sc_core::sc_spawn([&]() { s_kill2->start(sqr); });
    sc_core::wait(5, sc_core::SC_NS);
    hk2.kill(sc_core::SC_INCLUDE_DESCENDANTS);
    sc_core::wait(5, sc_core::SC_NS);
    std::cout << sc_core::sc_time_stamp() << ": D2 after proc.kill active="
              << s_kill2->active_watchers << std::endl;

    rel_seq* s_ok = rel_seq::type_id::create("s_ok");
    s_ok->value = 70;
    s_ok->relevant = true;
    s_ok->start(sqr);

    // Scenario E1: self-kill inside wait_for_relevant() returning immediately
    rel_seq* s_self1 = rel_seq::type_id::create("s_self1");
    s_self1->value = 80;
    s_self1->relevant = false;
    s_self1->self_kill_in_wait = true;
    sc_core::sc_process_handle he1 = sc_core::sc_spawn([&]() { s_self1->start(sqr); });
    sc_core::wait(he1.terminated_event());
    std::cout << sc_core::sc_time_stamp() << ": E1 self-kill term=" << he1.terminated()
              << " state=" << s_self1->get_sequence_state()
              << " active=" << s_self1->active_watchers << std::endl;

    // Scenario E2: self-kill inside wait_for_relevant() followed by wait(never)
    rel_seq* s_self2 = rel_seq::type_id::create("s_self2");
    s_self2->value = 81;
    s_self2->relevant = false;
    s_self2->self_kill_in_wait = true;
    s_self2->self_kill_then_wait = true;
    sc_core::sc_process_handle he2 = sc_core::sc_spawn([&]() { s_self2->start(sqr); });
    sc_core::wait(he2.terminated_event());
    sc_core::wait(sc_core::SC_ZERO_TIME);
    std::cout << sc_core::sc_time_stamp() << ": E2 self-kill-wait term=" << he2.terminated()
              << " state=" << s_self2->get_sequence_state()
              << " active=" << s_self2->active_watchers << std::endl;

    // Scenario F: stop_sequences() inside wait_for_relevant() with 2 blocked irrelevant seqs
    rel_seq* s_hold_f = rel_seq::type_id::create("s_hold_f");
    s_hold_f->value = 85;
    s_hold_f->locking = true;
    rel_seq* s_stop1 = rel_seq::type_id::create("s_stop1");
    s_stop1->value = 86;
    s_stop1->relevant = false;
    s_stop1->stop_sqr_in_wait = true;
    rel_seq* s_stop2 = rel_seq::type_id::create("s_stop2");
    s_stop2->value = 87;
    s_stop2->relevant = false;
    s_stop2->forever_wait = true;

    sc_core::sc_process_handle hhf = sc_core::sc_spawn([&]() { s_hold_f->start(sqr); });
    sc_core::wait(1, sc_core::SC_NS);
    sc_core::sc_process_handle hf1 = sc_core::sc_spawn([&]() { s_stop1->start(sqr); });
    sc_core::sc_process_handle hf2 = sc_core::sc_spawn([&]() { s_stop2->start(sqr); });
    sc_core::wait(hhf.terminated_event());
    if (hf1.valid() && !hf1.terminated())
      sc_core::wait(hf1.terminated_event());
    if (hf2.valid() && !hf2.terminated())
      sc_core::wait(hf2.terminated_event());
    sc_core::wait(sc_core::SC_ZERO_TIME);
    std::cout << sc_core::sc_time_stamp() << ": F stop_sequences term="
              << hf1.terminated() << "," << hf2.terminated()
              << " states=" << s_stop1->get_sequence_state() << "," << s_stop2->get_sequence_state()
              << " active=" << s_stop1->active_watchers << "," << s_stop2->active_watchers << std::endl;

    // Scenario F2: s_stop2b enters wait_for_relevant() first, then s_stop1b calls stop_sequences()
    rel_seq* s_hold_f2 = rel_seq::type_id::create("s_hold_f2");
    s_hold_f2->value = 88;
    s_hold_f2->locking = true;
    rel_seq* s_stop2b = rel_seq::type_id::create("s_stop2b");
    s_stop2b->value = 89;
    s_stop2b->relevant = false;
    s_stop2b->forever_wait = true;
    rel_seq* s_stop1b = rel_seq::type_id::create("s_stop1b");
    s_stop1b->value = 90;
    s_stop1b->relevant = false;
    s_stop1b->stop_sqr_in_wait = true;

    sc_core::sc_process_handle hhf2 = sc_core::sc_spawn([&]() { s_hold_f2->start(sqr); });
    sc_core::wait(1, sc_core::SC_NS);
    sc_core::sc_process_handle hf2b = sc_core::sc_spawn([&]() { s_stop2b->start(sqr); });
    sc_core::sc_process_handle hf1b = sc_core::sc_spawn([&]() { s_stop1b->start(sqr); });
    sc_core::wait(hhf2.terminated_event());
    if (hf2b.valid() && !hf2b.terminated())
      sc_core::wait(hf2b.terminated_event());
    if (hf1b.valid() && !hf1b.terminated())
      sc_core::wait(hf1b.terminated_event());
    sc_core::wait(sc_core::SC_ZERO_TIME);
    std::cout << sc_core::sc_time_stamp() << ": F2 stop_sequences term="
              << hf2b.terminated() << "," << hf1b.terminated()
              << " states=" << s_stop2b->get_sequence_state() << "," << s_stop1b->get_sequence_state()
              << " active=" << s_stop2b->active_watchers << "," << s_stop1b->active_watchers << std::endl;

    rel_seq* s_surv = rel_seq::type_id::create("s_surv");
    s_surv->value = 90;
    s_surv->relevant = true;
    s_surv->start(sqr);

    std::cout << "sequencer is " << (sqr->is_grabbed() ? "still locked" : "free") << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
