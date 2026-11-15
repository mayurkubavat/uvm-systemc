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

// Test of the weighted sequencer arbitration (SEQ_ARB_WEIGHTED). Four
// sequences send items to one sequencer at the same time. The last one
// has five times the priority of each of the others, so it must get
// most of the grants, and each of the others must get a grant as well
// while the last one is still waiting.

#include <systemc>
#include <uvm>

#include "sysc/kernel/sc_dynamic_processes.h"

using namespace uvm;

const int NUM_SEQS  = 4;
const int NUM_ITEMS = 100;
const int WINDOW    = 100; // grants looked at; all sequences still have items

class item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(item);

  int seq_no;

  item( const std::string& name = "item" ) : uvm_sequence_item(name), seq_no(0)
  {}
};

class item_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(item_seq);

  int seq_no;

  item_seq( const std::string& name = "item_seq" )
    : uvm_sequence<item>(name), seq_no(0)
  {}

  void body()
  {
    for (int i = 0; i < NUM_ITEMS; i++)
    {
      item* req = item::type_id::create("req");
      req->seq_no = seq_no;
      start_item(req);
      finish_item(req);
      item::type_id::destroy(req);
    }
  }
};

class top_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(top_seq);

  item_seq* seq[NUM_SEQS];

  top_seq( const std::string& name = "top_seq" ) : uvm_sequence<item>(name)
  {}

  void start_seq( int i )
  {
    // priorities 100, 100, 100 and 500
    seq[i]->start(m_sequencer, this, (i == NUM_SEQS - 1) ? 500 : 100);
  }

  void body()
  {
    for (int i = 0; i < NUM_SEQS; i++)
    {
      seq[i] = item_seq::type_id::create("seq");
      seq[i]->seq_no = i;
    }

    m_sequencer->set_arbitration(SEQ_ARB_WEIGHTED);

    SC_FORK
      sc_core::sc_spawn(sc_bind(&top_seq::start_seq, this, 0)),
      sc_core::sc_spawn(sc_bind(&top_seq::start_seq, this, 1)),
      sc_core::sc_spawn(sc_bind(&top_seq::start_seq, this, 2)),
      sc_core::sc_spawn(sc_bind(&top_seq::start_seq, this, 3))
    SC_JOIN

    for (int i = 0; i < NUM_SEQS; i++)
      item_seq::type_id::destroy(seq[i]);
  }
};

class driver : public uvm_driver<item>
{
 public:
  UVM_COMPONENT_UTILS(driver);

  int total;
  int grants[NUM_SEQS]; // grants in the first WINDOW items

  driver( uvm_component_name name ) : uvm_driver<item>(name), total(0)
  {
    for (int i = 0; i < NUM_SEQS; i++)
      grants[i] = 0;
  }

  void run_phase( uvm_phase& phase )
  {
    item req;

    while (true)
    {
      seq_item_port->get(req);
      if (total < WINDOW)
        grants[req.seq_no]++;
      total++;
      sc_core::wait(10, sc_core::SC_NS);
    }
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  driver* drv;
  uvm_sequencer<item>* sqr;

  test( uvm_component_name name ) : uvm_test(name), drv(nullptr), sqr(nullptr)
  {}

  void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    drv = driver::type_id::create("drv", this);
    sqr = new uvm_sequencer<item>("sqr");
  }

  void connect_phase( uvm_phase& phase )
  {
    drv->seq_item_port.connect(sqr->seq_item_export);
  }

  void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    top_seq* seq = top_seq::type_id::create("seq");
    seq->start(sqr);
    sc_core::wait(10, sc_core::SC_NS);
    top_seq::type_id::destroy(seq);
    phase.drop_objection(this);
  }

  void report_phase( uvm_phase& phase )
  {
    bool pass = (drv->total == NUM_SEQS * NUM_ITEMS);

    for (int i = 0; i < NUM_SEQS; i++)
    {
      std::cout << "sequence " << i << ": " << drv->grants[i]
                << " of the first " << WINDOW << " grants" << std::endl;
      if (drv->grants[i] == 0)
        pass = false;
      if (i < NUM_SEQS - 1 && drv->grants[i] >= drv->grants[NUM_SEQS - 1])
        pass = false;
    }

    if (pass)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "** UVM TEST FAILED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");

  return 0;
}
