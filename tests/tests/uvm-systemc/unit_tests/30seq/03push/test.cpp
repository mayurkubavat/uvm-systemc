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

// A push sequencer selects the items of its sequences and puts them to a
// push driver, which sends responses back. Arbitration, locks and
// responses work as with a pull sequencer.

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

class driver : public uvm_push_driver<item>
{
 public:
  UVM_COMPONENT_UTILS(driver);

  driver( uvm_component_name name ) : uvm_push_driver<item>(name) {}

  virtual void put( const item& t )
  {
    sc_core::wait(10, sc_core::SC_NS);
    std::cout << sc_core::sc_time_stamp() << ": driver got item " << t.value << std::endl;

    item r;
    r.set_id_info(const_cast<item&>(t));
    r.value = t.value + 1000;
    rsp_port.write(r);
  }
};

// Sends 'count' items from 'first' after 'delay' ns, holding the lock of
// the sequencer with 'locking', and checks the response of each item
class seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(seq);

  int first;
  int count;
  int delay;
  bool locking;
  int good_responses;

  seq( const std::string& name = "seq" )
  : uvm_sequence<item>(name), first(0), count(1), delay(0), locking(false), good_responses(0) {}

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

      item rsp;
      get_response(&rsp);
      if (rsp.value == req->value + 1000)
        good_responses++;

      item::type_id::destroy(req);
    }

    if (locking) m_sequencer->unlock(this);
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_push_sequencer<item>* sqr;
  driver* drv;
  seq* a;
  seq* b;

  test( uvm_component_name name ) : uvm_test(name), sqr(nullptr), drv(nullptr), a(nullptr), b(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    sqr = new uvm_push_sequencer<item>("sqr");
    drv = driver::type_id::create("drv", this);
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    sqr->req_port.connect(drv->req_export);
    drv->rsp_port.connect(sqr->rsp_export);
  }

  void run_a() { a->start(sqr); }
  void run_b() { b->start(sqr); }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    a = seq::type_id::create("a");
    a->first = 100; a->count = 3;

    // starts while 'a' runs and takes the lock: its items go first
    b = seq::type_id::create("b");
    b->first = 200; b->count = 2; b->delay = 5; b->locking = true;

    sc_core::sc_process_handle ha = sc_core::sc_spawn(sc_bind(&test::run_a, this));
    sc_core::sc_process_handle hb = sc_core::sc_spawn(sc_bind(&test::run_b, this));

    sc_core::wait(ha.terminated_event() & hb.terminated_event());

    std::cout << sc_core::sc_time_stamp() << ": good responses a " << a->good_responses
              << " of 3, b " << b->good_responses << " of 2" << std::endl;
    std::cout << "sequencer is " << (sqr->is_grabbed() ? "still locked" : "free") << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  run_test("test");
  return 0;
}
