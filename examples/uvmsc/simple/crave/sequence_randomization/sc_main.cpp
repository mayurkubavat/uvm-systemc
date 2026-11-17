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
// and rerandomize set. UVM_OBJECT_UTILS makes UVM-SystemC call the
// randomize() of the item and of the sequence, which they inherit from
// crave::crv_sequence_item through the adapters of crave2uvm.
//
// CRAVE is seeded from the global seed of UVM, so the values are the
// same in every run and change with the command line argument
// +UVM_SEED. Without arguments the seed is 4711. The generated values
// depend on the solver and the platform, so the example does not print
// them: it checks that they meet the constraints, that the number of
// items is within its constraint, that the items are randomized, and
// that the same seed generates the same values and another seed other
// values, and prints the results of these checks.

#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

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

typedef std::pair<unsigned int, unsigned int> item_values;

class driver : public uvm::uvm_driver<item>
{
 public:
  UVM_COMPONENT_UTILS(driver);

  unsigned int violations;
  std::vector<item_values> received;

  driver( uvm::uvm_component_name name )
  : uvm::uvm_driver<item>(name), violations(0)
  {}

  void run_phase( uvm::uvm_phase& phase )
  {
    item req;

    while (true)
    {
      seq_item_port->get_next_item(req);
      received.push_back(item_values(req.addr, req.data));
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

  // the number of items of the last sequence started
  static unsigned int last_num_items;

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
    last_num_items = num_items;

    std::cout << "items with UVM_DO" << std::endl;
    for (unsigned int i = 0; i < num_items; i++)
    {
      item* req = nullptr;
      UVM_DO(req);
    }

    std::cout << "items with send_request" << std::endl;
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

unsigned int sequence::last_num_items = 0;

// the values of count items randomized after CRAVE is seeded with seed;
// called outside of a SystemC process, where CRAVE uses the generator
// which set_global_seed seeds
static std::vector<item_values> generate( unsigned int seed, int count )
{
  crave::set_global_seed(seed);
  item it("generated");
  std::vector<item_values> values;
  for (int i = 0; i < count; i++)
  {
    if (!it.randomize())
      break;
    values.push_back(item_values(it.addr, it.data));
  }
  return values;
}

static void check( bool ok, const std::string& what )
{
  std::cout << what << ": " << (ok ? "ok" : "FAILED") << std::endl;
  if (!ok)
    UVM_ERROR("CHECK", what);
}

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
    const unsigned int n = sequence::last_num_items;
    const std::vector<item_values>& received = drv->received;
    std::set<item_values> distinct(received.begin(), received.end());

    check(drv->violations == 0, "items meet their constraints");
    check(n >= 2 && n <= 5, "number of items meets its constraint");
    check(received.size() == 2 * n, "items received with UVM_DO and send_request");
    check(distinct.size() > 1, "items are randomized");
  }

  // the end of the test does not depend on the number of items
  void main_phase( uvm::uvm_phase& phase )
  {
    phase.raise_objection(this);
    sc_core::wait(200, sc_core::SC_NS);
    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  // read the settings of CRAVE; the file is created if it does not exist
  crave::init("crave.cfg");

  uvm::run_test("test");

  // the same seed generates the same values, another seed other values
  // (crave::set_global_seed ignores the seed 0)
  const unsigned int seed = uvm::uvm_get_random_seed() != 0 ? uvm::uvm_get_random_seed() : 1;
  const unsigned int other_seed = seed + 1 != 0 ? seed + 1 : 1;
  const std::vector<item_values> first = generate(seed, 8);
  check(first.size() == 8, "randomization succeeds");
  check(generate(seed, 8) == first, "the same seed generates the same values");
  check(generate(other_seed, 8) != first, "another seed generates other values");

  return 0;
}

int main( int argc, char* argv[] )
{
  if (argc > 1)
    return sc_core::sc_elab_and_sim(argc, argv);

  const char* args[] = { argv[0], "+UVM_SEED=4711" };
  return sc_core::sc_elab_and_sim(2, const_cast<char**>(args));
}
