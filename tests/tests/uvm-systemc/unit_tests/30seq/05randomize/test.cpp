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

// Items and sequences which override uvm_sequence_item::randomize() are
// randomized where UVM-SV calls randomize():
// - send_request() with rerandomize set,
// - the macro UVM_DO, after the grant (between pre_do and mid_do) for
//   an item, and before start() for a sub-sequence unless
//   do_not_randomize is set,
// - the default sequence of a phase, before the sequencer starts it,
//   unless do_not_randomize is set.
// A failed randomization is reported with a warning. The constraints
// of UVM_DO_WITH are not applied, which is reported once per use.
// Classes whose randomize() returns nothing, or is not accessible, still
// compile and are not randomized.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

// Gives deterministic "random" values, so the log does not depend on a
// constraint solver.
static int next_value = 100;

class item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(item);

  int value;
  bool fail;

  item( const std::string& name = "item" )
  : uvm_sequence_item(name), value(0), fail(false) {}

  virtual bool randomize()
  {
    if (fail)
    {
      std::cout << sc_core::sc_time_stamp() << ": randomize " << get_name()
                << " fails" << std::endl;
      return false;
    }
    value = next_value++;
    std::cout << sc_core::sc_time_stamp() << ": randomize " << get_name()
              << " to " << value << std::endl;
    return true;
  }
};

// compiles, and is not randomized
class void_item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(void_item);

  void_item( const std::string& name = "void_item" ) : uvm_sequence_item(name) {}

  void randomize() {}
};

class private_item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(private_item);

  private_item( const std::string& name = "private_item" ) : uvm_sequence_item(name) {}

 private:
  bool randomize() { return false; }
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
      std::cout << sc_core::sc_time_stamp() << ": " << get_name()
                << " got value " << req.value << std::endl;
      sc_core::wait(10, sc_core::SC_NS);
      seq_item_port->item_done();
    }
  }
};

// Sends an item with send_request(), with and without rerandomize.
class send_request_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(send_request_seq);

  send_request_seq( const std::string& name = "send_request_seq" )
  : uvm_sequence<item>(name) {}

  void send( item* req, bool rerandomize )
  {
    wait_for_grant();
    send_request(req, rerandomize);
    wait_for_item_done();
  }

  virtual void body()
  {
    item* req = item::type_id::create("req");
    req->value = 1;
    send(req, false);
    send(req, true);
    std::cout << sc_core::sc_time_stamp() << ": value of req in the sequence "
              << req->value << std::endl;
    req->fail = true;
    send(req, true);
    item::type_id::destroy(req);
  }
};

// A sub-sequence with a randomized number of items.
class count_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(count_seq);

  int count;

  count_seq( const std::string& name = "count_seq" )
  : uvm_sequence<item>(name), count(1) {}

  virtual bool randomize()
  {
    count = 2;
    std::cout << sc_core::sc_time_stamp() << ": randomize " << get_name()
              << " to count " << count << std::endl;
    return true;
  }

  virtual void body()
  {
    std::cout << sc_core::sc_time_stamp() << ": " << get_name()
              << " sends " << count << " items" << std::endl;
    for (int i = 0; i < count; i++)
    {
      item* req = nullptr;
      UVM_DO(req);
    }
  }
};

// A sub-sequence which is not randomized.
class fixed_count_seq : public count_seq
{
 public:
  UVM_OBJECT_UTILS(fixed_count_seq);

  fixed_count_seq( const std::string& name = "fixed_count_seq" )
  : count_seq(name)
  {
    do_not_randomize = true;
  }
};

// Uses UVM_DO for items and sub-sequences.
class do_seq : public uvm_sequence<item>
{
 public:
  UVM_OBJECT_UTILS(do_seq);

  do_seq( const std::string& name = "do_seq" ) : uvm_sequence<item>(name) {}

  virtual void pre_do( bool is_item )
  {
    if (is_item)
      std::cout << sc_core::sc_time_stamp() << ": pre_do" << std::endl;
  }

  virtual void mid_do( uvm_sequence_item* this_item )
  {
    if (this_item->is_item())
      std::cout << sc_core::sc_time_stamp() << ": mid_do "
                << dynamic_cast<item*>(this_item)->value << std::endl;
  }

  virtual void body()
  {
    item* req = nullptr;
    UVM_DO(req);

    count_seq* sub = nullptr;
    UVM_DO(sub);

    fixed_count_seq* fixed_sub = nullptr;
    UVM_DO(fixed_sub);

    for (int i = 0; i < 2; i++)
    {
      item* req_with = nullptr;
      UVM_DO_WITH(req_with, req_with->value == 5);
    }

    void_item v;
    private_item p;
    std::cout << sc_core::sc_time_stamp() << ": randomize void_item "
              << v.m_uvm_randomize() << ", private_item "
              << p.m_uvm_randomize() << std::endl;
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_sequencer<item>* sqr;
  uvm_sequencer<item>* main_sqr;
  uvm_sequencer<item>* fixed_sqr;
  driver* drv;
  driver* main_drv;
  driver* fixed_drv;

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    sqr = new uvm_sequencer<item>("sqr");
    main_sqr = new uvm_sequencer<item>("main_sqr");
    fixed_sqr = new uvm_sequencer<item>("fixed_sqr");
    drv = driver::type_id::create("drv", this);
    main_drv = driver::type_id::create("main_drv", this);
    fixed_drv = driver::type_id::create("fixed_drv", this);

    // default sequence by type: randomized before it is started
    uvm_config_db<uvm_object_wrapper*>::set(this, "main_sqr.main_phase",
      "default_sequence", count_seq::type_id::get());

    // default sequence instance which shall not be randomized
    count_seq* fixed = count_seq::type_id::create("fixed");
    fixed->count = 3;
    fixed->do_not_randomize = true;
    uvm_config_db<uvm_sequence_base*>::set(this, "fixed_sqr.main_phase",
      "default_sequence", fixed);
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    drv->seq_item_port.connect(sqr->seq_item_export);
    main_drv->seq_item_port.connect(main_sqr->seq_item_export);
    fixed_drv->seq_item_port.connect(fixed_sqr->seq_item_export);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // let the default sequences of the main phase finish first
    sc_core::wait(100, sc_core::SC_NS);

    std::cout << "send_request" << std::endl;
    send_request_seq* s1 = send_request_seq::type_id::create("s1");
    s1->start(sqr);

    std::cout << "UVM_DO" << std::endl;
    do_seq* s2 = do_seq::type_id::create("s2");
    s2->start(sqr);

    phase.drop_objection(this);
  }

  virtual void main_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    std::cout << "default sequences" << std::endl;
    sc_core::wait(50, sc_core::SC_NS);
    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  run_test("test");
  return 0;
}
