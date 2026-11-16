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

// UVM-SystemC randomizes items and sequences with CRAVE where UVM-SV
// calls randomize(): the default sequence of a phase before it is
// started, items sent with UVM_DO and items sent with send_request()
// and rerandomize set. uvm_randomized_sequence_item and
// uvm_randomized_sequence implement uvm_sequence_item::randomize() with
// the solver of CRAVE.
//
// CRAVE is seeded from the global seed of UVM, so the values are the
// same in every run and change with the command line argument
// +UVM_SEED. Without arguments the seed is 4711.

#include <iostream>

#include <systemc>
#include <uvm>

// uvm_randomized_sequence_item without the macros of crave2uvm.h, so
// that UVM_DO is the one of UVM-SystemC
#include <uvm_randomized_sequence.h>
#include <uvm_randomized_sequence_item.h>

class item : public uvm_randomized_sequence_item
{
 public:
  crave::crv_variable<unsigned int> addr;
  crave::crv_variable<unsigned int> data;

  crave::crv_constraint c_addr { addr() >= 0x10u, addr() <= 0x1fu };
  crave::crv_constraint c_data { data() < 0x100u, data() != addr() };

  UVM_OBJECT_UTILS(item);

  item( crave::crv_object_name name = "item" )
  : uvm_randomized_sequence_item(name)
  {}

  // The sequencer passes items by value: a copy only takes over the
  // generated values.
  item( const item& rhs ) : uvm_randomized_sequence_item(rhs.get_name())
  {
    *this = rhs;
  }

  item& operator=( const item& rhs )
  {
    if (this != &rhs)
    {
      uvm::uvm_sequence_item::operator=(rhs);
      addr = static_cast<unsigned int>(rhs.addr);
      data = static_cast<unsigned int>(rhs.data);
    }
    return *this;
  }

  bool valid() const
  {
    return addr >= 0x10u && addr <= 0x1fu && data < 0x100u && data != addr;
  }
};

class driver : public uvm::uvm_driver<item>
{
 public:
  UVM_COMPONENT_UTILS(driver);

  unsigned int violations;

  driver( uvm::uvm_component_name name )
  : uvm::uvm_driver<item>(name), violations(0)
  {}

  void run_phase( uvm::uvm_phase& phase )
  {
    item req;

    while (true)
    {
      seq_item_port->get_next_item(req);
      std::cout << sc_core::sc_time_stamp() << ": addr 0x" << std::hex
                << req.addr << " data 0x" << req.data << std::dec
                << std::endl;
      if (!req.valid())
        violations++;
      sc_core::wait(10, sc_core::SC_NS);
      seq_item_port->item_done();
    }
  }
};

class sequence : public uvm_randomized_sequence<item>
{
 public:
  crave::crv_variable<unsigned int> num_items;

  crave::crv_constraint c_num_items { num_items() >= 2u, num_items() <= 5u };

  UVM_OBJECT_UTILS(sequence);

  sequence( crave::crv_object_name name = "sequence" )
  : uvm_randomized_sequence<item>(name)
  {}

  void pre_body()
  {
    if (get_starting_phase() != nullptr)
      get_starting_phase()->raise_objection(this);
  }

  void post_body()
  {
    if (get_starting_phase() != nullptr)
      get_starting_phase()->drop_objection(this);
  }

  void body()
  {
    std::cout << sc_core::sc_time_stamp() << ": " << num_items
              << " items with UVM_DO" << std::endl;
    for (unsigned int i = 0; i < num_items; i++)
    {
      item* req = nullptr;
      UVM_DO(req);
    }

    std::cout << sc_core::sc_time_stamp() << ": " << num_items
              << " items with send_request" << std::endl;
    item* req = item::type_id::create("req");
    for (unsigned int i = 0; i < num_items; i++)
    {
      wait_for_grant();
      send_request(req, true);
      wait_for_item_done();
    }
    item::type_id::destroy(req);
  }
};

class test : public uvm::uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm::uvm_sequencer<item>* sqr;
  driver* drv;

  test( uvm::uvm_component_name name )
  : uvm::uvm_test(name), sqr(nullptr), drv(nullptr)
  {}

  void build_phase( uvm::uvm_phase& phase )
  {
    uvm::uvm_test::build_phase(phase);

    crave::set_global_seed(uvm::uvm_get_random_seed());
    std::cout << "seed " << uvm::uvm_get_random_seed() << std::endl;

    sqr = new uvm::uvm_sequencer<item>("sqr");
    drv = driver::type_id::create("drv", this);

    uvm::uvm_config_db<uvm::uvm_object_wrapper*>::set(this, "sqr.main_phase",
      "default_sequence", sequence::type_id::get());
  }

  void connect_phase( uvm::uvm_phase& phase )
  {
    drv->seq_item_port.connect(sqr->seq_item_export);
  }

  void report_phase( uvm::uvm_phase& phase )
  {
    std::cout << "constraint violations: " << drv->violations << std::endl;
  }
};

int sc_main( int, char*[] )
{
  // read the settings of CRAVE; the file is created if it does not exist
  crave::init("crave.cfg");

  uvm::run_test("test");

  return 0;
}

int main( int argc, char* argv[] )
{
  if (argc > 1)
    return sc_core::sc_elab_and_sim(argc, argv);

  const char* args[] = { argv[0], "+UVM_SEED=4711" };
  return sc_core::sc_elab_and_sim(2, const_cast<char**>(args));
}
