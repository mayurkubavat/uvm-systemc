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

// Reproduces and verifies Fix 3 (uvm_push_sequencer mid-grant kill recovery):
// - Mid-grant victim_seq->kill() while uvm_push_sequencer::run_phase waits
//   for item, recovering to serve survivor_seq and deliver its responses
// - Mid-grant direct sc_process_handle::kill() (zombie grant) recovery

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

class push_driver : public uvm_push_driver<item>
{
 public:
  UVM_COMPONENT_UTILS(push_driver);

  push_driver( uvm_component_name name ) : uvm_push_driver<item>(name) {}

  virtual void put( const item& t )
  {
    sc_core::wait(10, sc_core::SC_NS);
    std::cout << sc_core::sc_time_stamp() << ": push_driver got item " << t.value << std::endl;

    item r;
    r.set_id_info(const_cast<item&>(t));
    r.value = t.value + 1000;
    rsp_port.write(r);
  }
};

class victim_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(victim_seq);

  int value;

  victim_seq( const std::string& name = "victim_seq" )
  : uvm_sequence<item>(name), value(999) {}

  virtual void pre_do( bool is_item )
  {
    if (is_item)
      sc_core::wait(5, sc_core::SC_NS);
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
  int good_responses;

  survivor_seq( const std::string& name = "survivor_seq" )
  : uvm_sequence<item>(name), first(200), count(2), good_responses(0) {}

  virtual void body()
  {
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
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_push_sequencer<item>* sqr;
  push_driver* drv;

  test( uvm_component_name name ) : uvm_test(name), sqr(nullptr), drv(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    sqr = new uvm_push_sequencer<item>("sqr");
    drv = push_driver::type_id::create("drv", this);
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    sqr->req_port.connect(drv->req_export);
    drv->rsp_port.connect(sqr->rsp_export);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // 1. Mid-grant victim_seq->kill() on uvm_push_sequencer
    {
      victim_seq* victim = victim_seq::type_id::create("victim");
      survivor_seq* surv = survivor_seq::type_id::create("surv1");
      surv->first = 200;
      surv->count = 2;

      sc_core::sc_process_handle hv = sc_core::sc_spawn([&]() { victim->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);
      sc_core::sc_process_handle hs = sc_core::sc_spawn([&]() { surv->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);

      victim->kill();
      if (hv.valid() && !hv.terminated())
        sc_core::wait(hv.terminated_event());
      sc_core::wait(hs.terminated_event());
      std::cout << sc_core::sc_time_stamp() << ": surv1 good_responses="
                << surv->good_responses << " of 2" << std::endl;
    }

    // 2. Mid-grant direct sc_process_handle::kill() on uvm_push_sequencer
    {
      victim_seq* zseq = victim_seq::type_id::create("zseq");
      survivor_seq* surv2 = survivor_seq::type_id::create("surv2");
      surv2->first = 300;
      surv2->count = 2;

      sc_core::sc_process_handle hz = sc_core::sc_spawn([&]() {
        sqr->wait_for_grant(zseq);
        sc_core::wait(5, sc_core::SC_NS);
      });
      sc_core::wait(1, sc_core::SC_NS);
      sc_core::sc_process_handle hs2 = sc_core::sc_spawn([&]() { surv2->start(sqr); });
      sc_core::wait(1, sc_core::SC_NS);

      hz.kill();
      sc_core::wait(hs2.terminated_event());
      std::cout << sc_core::sc_time_stamp() << ": surv2 good_responses="
                << surv2->good_responses << " of 2" << std::endl;
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
