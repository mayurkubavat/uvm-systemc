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

// A driver writes its responses to its rsp_port, which is connected to
// the rsp_export of the sequencer. The sequence shall get them with
// get_response(), as when the driver uses put_response().

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
      seq_item_port->item_done();

      item rsp;
      rsp.set_id_info(req);
      rsp.value = req.value * 2;
      rsp_port.write(rsp);
    }
  }
};

class seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(seq);

  seq( const std::string& name = "seq" ) : uvm_sequence<item>(name) {}

  virtual void body()
  {
    for (int i = 1; i <= 3; i++)
    {
      item* req = item::type_id::create("req");
      req->value = i;
      start_item(req);
      finish_item(req);

      item rsp;
      get_response(&rsp);
      std::cout << sc_core::sc_time_stamp() << ": response " << rsp.value
                << " for request " << req->value << std::endl;
      item::type_id::destroy(req);
    }
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
    drv->rsp_port.connect(sqr->rsp_export);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    seq* s = seq::type_id::create("s");
    s->start(sqr);
    std::cout << sc_core::sc_time_stamp() << ": all responses received, "
              << sqr->get_num_rsps_received() << " counted by the sequencer" << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  run_test("test");
  return 0;
}
