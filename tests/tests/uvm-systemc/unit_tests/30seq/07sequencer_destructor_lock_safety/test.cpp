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

// Reproduces and verifies Fix 2: ~uvm_sequencer_base() must not delete
// non-owning pointers in lock_list while still freeing arb_sequence_q.
// - Scenario A: stack-allocated sequence holds lock when sequencer is destroyed
// - Scenario B: std::shared_ptr-owned sequence holds lock + pending
//   arb_sequence_q request when sequencer is destroyed

#include <systemc>
#include <uvm>

#include <iostream>
#include <memory>
#include <string>

using namespace uvm;

class item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(item);

  int value;

  item( const std::string& name = "item" ) : uvm_sequence_item(name), value(0) {}
};

class lock_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(lock_seq);

  lock_seq( const std::string& name = "lock_seq" ) : uvm_sequence<item>(name) {}
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_sequencer<item>* sqr_a;
  uvm_sequencer<item>* sqr_b;

  test( uvm_component_name name ) : uvm_test(name), sqr_a(nullptr), sqr_b(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    sqr_a = new uvm_sequencer<item>("sqr_a");
    sqr_b = new uvm_sequencer<item>("sqr_b");
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // Scenario A: stack-allocated sequence holding lock at sequencer destruction
    {
      lock_seq s_stack("s_stack");
      sc_core::sc_process_handle h = sc_core::sc_spawn([&]() {
        sqr_a->lock(&s_stack);
      });
      sc_core::wait(5, sc_core::SC_NS);
      std::cout << "Scenario A: is_grabbed=" << sqr_a->is_grabbed() << std::endl;
      delete sqr_a;
      sqr_a = nullptr;
      if (h.valid() && !h.terminated())
        h.kill();
      std::cout << "Scenario A: sqr_a deleted safely" << std::endl;
    }

    // Scenario B: shared_ptr sequence holding lock + pending arb_sequence_q entry
    {
      std::shared_ptr<lock_seq> s_heap = std::make_shared<lock_seq>("s_heap");
      lock_seq* s_waiter = lock_seq::type_id::create("s_waiter");

      sc_core::sc_process_handle h1 = sc_core::sc_spawn([&]() {
        sqr_b->lock(s_heap.get());
      });
      sc_core::wait(1, sc_core::SC_NS);

      sc_core::sc_process_handle h2 = sc_core::sc_spawn([&]() {
        sqr_b->wait_for_grant(s_waiter);
      });
      sc_core::wait(5, sc_core::SC_NS);
      std::cout << "Scenario B: is_grabbed=" << sqr_b->is_grabbed() << std::endl;

      // Kill waiting process before destroying sequencer event so SystemC
      // does not report premature sc_event destruction; s_waiter's request
      // stays in arb_sequence_q and must be freed by ~uvm_sequencer_base().
      if (h2.valid() && !h2.terminated())
        h2.kill();
      if (h1.valid() && !h1.terminated())
        h1.kill();

      delete sqr_b;
      sqr_b = nullptr;
      s_heap.reset();
      lock_seq::type_id::destroy(s_waiter);
      std::cout << "Scenario B: sqr_b and s_heap cleaned safely" << std::endl;
    }

    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  run_test("test");
  return 0;
}
