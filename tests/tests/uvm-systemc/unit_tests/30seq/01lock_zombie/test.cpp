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

// A process which waits for a lock, or for a grant, on a sequencer is
// killed before it gets it. The sequencer shall report the request,
// drop it, and carry on with the other sequences, as in UVM-SV. Without
// this, the lock is granted to nobody and the sequencer is blocked.

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

// Sends 'count' items, the first after 'delay' ns; with 'locking' it holds
// the lock of the sequencer while doing so
class seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(seq);

  int first;
  int count;
  int delay;
  bool locking;

  seq( const std::string& name = "seq" )
  : uvm_sequence<item>(name), first(0), count(1), delay(0), locking(false) {}

  virtual void body()
  {
    sc_core::wait(delay, sc_core::SC_NS);

    if (locking) m_sequencer->lock(this);

    for (int i = 0; i < count; i++)
    {
      item* req = item::type_id::create("req");
      req->value = first + i;
      start_item(req);
      finish_item(req);
      item::type_id::destroy(req);
    }

    if (locking) m_sequencer->unlock(this);
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_sequencer<item>* sqr;
  driver* drv;
  seq* holder;
  seq* other;
  seq* lock_zombie;
  seq* grant_zombie;

  test( uvm_component_name name ) : uvm_test(name) {}

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

  void run_holder() { holder->start(sqr); }
  void run_other()  { other->start(sqr); }
  void run_again()  { lock_zombie->start(sqr); }

  // these two wait behind the lock of 'holder' and are killed while waiting
  void wait_for_lock()
  {
    sqr->lock(lock_zombie);
    std::cout << "the killed process got the lock" << std::endl;
  }

  void wait_for_a_grant()
  {
    sqr->wait_for_grant(grant_zombie);
    std::cout << "the killed process got a grant" << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    holder = seq::type_id::create("holder");
    holder->first = 100; holder->count = 3; holder->locking = true;

    other = seq::type_id::create("other");
    other->first = 200; other->count = 2; other->delay = 15;

    lock_zombie = seq::type_id::create("lock_zombie");
    grant_zombie = seq::type_id::create("grant_zombie");

    sc_core::sc_process_handle h1 = sc_core::sc_spawn(sc_bind(&test::run_holder, this));
    sc_core::sc_process_handle h2 = sc_core::sc_spawn(sc_bind(&test::run_other, this));

    sc_core::wait(5, sc_core::SC_NS);
    sc_core::sc_process_handle z1 = sc_core::sc_spawn(sc_bind(&test::wait_for_lock, this));
    sc_core::sc_process_handle z2 = sc_core::sc_spawn(sc_bind(&test::wait_for_a_grant, this));

    sc_core::wait(5, sc_core::SC_NS);
    z1.kill();
    z2.kill();
    std::cout << sc_core::sc_time_stamp() << ": the two waiting processes were killed" << std::endl;

    // The sequence of a killed process is started again. The sequencer
    // shall drop the old request once, and not look at it again.
    lock_zombie->first = 300;
    sc_core::sc_process_handle h3 = sc_core::sc_spawn(sc_bind(&test::run_again, this));

    // enough time for all five items, 10 ns each
    sc_core::wait(200, sc_core::SC_NS);

    std::cout << "sequencer is " << (sqr->is_grabbed() ? "still locked" : "free") << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
