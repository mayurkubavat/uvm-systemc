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

// A sequence library runs the sequences which were added to it, by type
// (UVM_ADD_TO_SEQ_LIB, add_typewide_sequence) or by instance
// (add_sequence), in each of its selection modes, and takes its
// configuration from the configuration database as in UVM-SV.

#include <systemc>
#include <uvm>

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace uvm;

class item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(item);

  std::string source;

  item( const std::string& name = "item" ) : uvm_sequence_item(name), source("library") {}
};

std::vector<std::string> driven;

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
      driven.push_back(req.source);
      seq_item_port->item_done();
    }
  }
};

class seq_lib;
class other_lib;

class seq_base : public uvm_sequence<item>
{
 public:
  seq_base( const std::string& name ) : uvm_sequence<item>(name) {}

  virtual void body()
  {
    item* req = item::type_id::create("req");
    req->source = get_type_name();
    start_item(req);
    finish_item(req);
    delete req;
  }
};

class seq_lib : public uvm_sequence_library<item>
{
 public:
  UVM_OBJECT_UTILS(seq_lib);
  UVM_SEQUENCE_LIBRARY_UTILS(seq_lib);

  seq_lib( const std::string& name = "seq_lib" ) : uvm_sequence_library<item>(name)
  {
    init_sequence_library();
  }

  // the members which a test may look at
  unsigned int executed() const { return sequences_executed; }
  int distrib( const std::string& name ) { return seqs_distrib[name]; }
};

// A second library type, which shall not get the sequences of the first
class other_lib : public uvm_sequence_library<item>
{
 public:
  UVM_OBJECT_UTILS(other_lib);
  UVM_SEQUENCE_LIBRARY_UTILS(other_lib);

  other_lib( const std::string& name = "other_lib" ) : uvm_sequence_library<item>(name)
  {
    init_sequence_library();
  }
};

// A library with its own selection: always the last sequence
class user_lib : public seq_lib
{
 public:
  UVM_OBJECT_UTILS(user_lib);

  user_lib( const std::string& name = "user_lib" ) : seq_lib(name) {}

  virtual unsigned int select_sequence( unsigned int max ) { return max; }
};

class seq_a : public seq_base
{
 public:
  UVM_OBJECT_UTILS(seq_a);
  UVM_ADD_TO_SEQ_LIB(seq_a, seq_lib);

  seq_a( const std::string& name = "seq_a" ) : seq_base(name) {}
};

class seq_b : public seq_base
{
 public:
  UVM_OBJECT_UTILS(seq_b);
  UVM_ADD_TO_SEQ_LIB(seq_b, seq_lib);
  UVM_ADD_TO_SEQ_LIB(seq_b, other_lib);

  seq_b( const std::string& name = "seq_b" ) : seq_base(name) {}
};

class seq_c : public seq_base
{
 public:
  UVM_OBJECT_UTILS(seq_c);

  seq_c( const std::string& name = "seq_c" ) : seq_base(name) {}
};

class seq_d : public seq_base
{
 public:
  UVM_OBJECT_UTILS(seq_d);

  seq_d( const std::string& name = "seq_d" ) : seq_base(name) {}
};

// not a sequence
class plain : public uvm::uvm_object
{
 public:
  UVM_OBJECT_UTILS(plain);

  plain( const std::string& name = "plain" ) : uvm::uvm_object(name) {}
};

// The messages of the library which the test provokes on purpose
class demoter : public uvm_report_catcher
{
 public:
  demoter() : uvm_report_catcher("demoter") {}

  virtual action_e do_catch()
  {
    if (get_id().find("SEQ") == 0 && get_severity() != UVM_INFO)
    {
      std::cout << "caught " << get_id() << std::endl;
      return CAUGHT;
    }
    return THROW;
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_sequencer<item>* sqr;
  uvm_sequencer<item>* sqr_cfg;
  uvm_sequencer<item>* sqr_fields;
  uvm_sequencer<>* sqr_base;
  driver* drv;
  driver* drv_cfg;
  driver* drv_fields;
  int errors;
  demoter catcher;

  test( uvm_component_name name )
  : uvm_test(name), sqr(nullptr), sqr_cfg(nullptr), sqr_fields(nullptr), sqr_base(nullptr),
    drv(nullptr), drv_cfg(nullptr), drv_fields(nullptr), errors(0) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    sqr = new uvm_sequencer<item>("sqr");
    sqr_cfg = new uvm_sequencer<item>("sqr_cfg");
    sqr_fields = new uvm_sequencer<item>("sqr_fields");
    sqr_base = new uvm_sequencer<>("sqr_base");
    drv = driver::type_id::create("drv", this);
    drv_cfg = driver::type_id::create("drv_cfg", this);
    drv_fields = driver::type_id::create("drv_fields", this);

    uvm_config_db<uvm_sequence_library_cfg*>::set(this, "sqr_cfg", "default_sequence.config",
      new uvm_sequence_library_cfg("cfg", UVM_SEQ_LIB_RANDC, 4, 4));

    uvm_config_db<unsigned int>::set(this, "sqr_fields", "default_sequence.min_random_count", 3);
    uvm_config_db<unsigned int>::set(this, "sqr_fields", "default_sequence.max_random_count", 5);
    uvm_config_db<uvm_sequence_lib_mode>::set(this, "sqr_fields",
      "default_sequence.selection_mode", UVM_SEQ_LIB_USER);
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    drv->seq_item_port.connect(sqr->seq_item_export);
    drv_cfg->seq_item_port.connect(sqr_cfg->seq_item_export);
    drv_fields->seq_item_port.connect(sqr_fields->seq_item_export);
  }

  void check( bool ok, const std::string& what )
  {
    std::cout << (ok ? "ok: " : "FAILED: ") << what << std::endl;
    if (!ok) errors++;
  }

  std::map<std::string, int> count_driven()
  {
    std::map<std::string, int> counts;
    for (unsigned int i = 0; i < driven.size(); i++)
      counts[driven[i]]++;
    driven.clear();
    return counts;
  }

  void show_driven()
  {
    std::cout << "driven:";
    for (unsigned int i = 0; i < driven.size(); i++)
      std::cout << " " << driven[i];
    std::cout << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    uvm_report_cb::add(nullptr, &catcher);

    std::vector<uvm_object_wrapper*> types;
    std::map<std::string, int> counts;

    // sequences added by type
    seq_lib* lib = seq_lib::type_id::create("lib");
    lib->get_sequences(types);
    check(types.size() == 2, "the library has the two sequences which were added by type");
    check(lib->get_sequence(0) == seq_a::get_type() && lib->get_sequence(1) == seq_b::get_type(),
      "the sequences are in the order in which they were added");

    other_lib* other = other_lib::type_id::create("other");
    types.clear();
    other->get_sequences(types);
    check(types.size() == 1 && types[0] == seq_b::get_type(),
      "another library type has its own sequences");

    // sequences added by instance, and those which are not added
    lib->add_sequence(seq_c::get_type());
    lib->add_sequence(seq_c::get_type());
    lib->add_sequence(seq_a::get_type());
    lib->add_sequence(plain::get_type());
    lib->add_sequence(nullptr);
    types.clear();
    lib->get_sequences(types);
    check(types.size() == 3, "a sequence is added once, and only a sequence is added");
    check(lib->get_sequence(3) == nullptr, "there is no sequence beyond the last one");

    seq_lib* lib2 = seq_lib::type_id::create("lib2");
    types.clear();
    lib2->get_sequences(types);
    check(types.size() == 2, "a sequence added to one library is not in the next one");

    // random cyclic selection: every sequence equally often
    lib->selection_mode = UVM_SEQ_LIB_RANDC;
    lib->min_random_count = 6;
    lib->max_random_count = 6;
    lib->start(sqr);
    show_driven();
    counts = count_driven();
    check(lib->sequence_count == 6 && lib->executed() == 6, "six sequences were executed");
    check(counts["seq_a"] == 2 && counts["seq_b"] == 2 && counts["seq_c"] == 2,
      "random cyclic selection runs every sequence twice in six");
    check(lib->distrib("seq_a") == 2 && lib->distrib("seq_c") == 2,
      "the library counts the sequences it executed");

    // random selection: a number of sequences within the limits
    lib2->min_random_count = 5;
    lib2->max_random_count = 15;
    lib2->start(sqr);
    show_driven();
    counts = count_driven();
    check(lib2->sequence_count >= 5 && lib2->sequence_count <= 15,
      "the number of sequences is between the minimum and the maximum");
    check(static_cast<unsigned int>(counts["seq_a"] + counts["seq_b"]) == lib2->sequence_count
          && counts["seq_a"] > 0 && counts["seq_b"] > 0,
      "random selection runs the sequences of the library");

    // a sequence which is removed is not selected any more
    lib->remove_sequence(seq_b::get_type());
    lib->selection_mode = UVM_SEQ_LIB_USER;
    lib->min_random_count = 5;
    lib->max_random_count = 5;
    lib->start(sqr);
    show_driven();
    counts = count_driven();
    check(counts["seq_a"] == 3 && counts["seq_c"] == 2 && counts["seq_b"] == 0,
      "user selection goes through the sequences in turn by default");

    // items only
    lib->selection_mode = UVM_SEQ_LIB_ITEM;
    lib->min_random_count = 3;
    lib->max_random_count = 3;
    lib->start(sqr);
    show_driven();
    counts = count_driven();
    check(counts["library"] == 3 && counts.size() == 1, "item mode sends items, no sequences");

    // an own selection
    user_lib* user = user_lib::type_id::create("user");
    user->selection_mode = UVM_SEQ_LIB_USER;
    user->min_random_count = 2;
    user->max_random_count = 2;
    user->start(sqr);
    show_driven();
    counts = count_driven();
    check(counts["seq_b"] == 2 && counts.size() == 1, "select_sequence can be overridden");

    // configuration by a configuration object
    seq_lib* lib_cfg = seq_lib::type_id::create("lib_cfg");
    lib_cfg->add_sequence(seq_c::get_type());
    lib_cfg->add_sequence(seq_d::get_type());
    lib_cfg->start(sqr_cfg);
    show_driven();
    counts = count_driven();
    check(lib_cfg->selection_mode == UVM_SEQ_LIB_RANDC && lib_cfg->sequence_count == 4,
      "the library takes its configuration object from the database");
    check(counts["seq_a"] == 1 && counts["seq_b"] == 1 && counts["seq_c"] == 1 && counts["seq_d"] == 1,
      "four sequences in random cyclic mode are four different ones");

    // configuration by fields
    seq_lib* lib_fields = seq_lib::type_id::create("lib_fields");
    lib_fields->start(sqr_fields);
    show_driven();
    counts = count_driven();
    check(lib_fields->selection_mode == UVM_SEQ_LIB_USER && lib_fields->min_random_count == 3
          && lib_fields->max_random_count == 5,
      "the library takes its fields from the database");
    check(lib_fields->sequence_count >= 3 && lib_fields->sequence_count <= 5,
      "the number of sequences is within the configured limits");

    // wrong limits
    lib2->min_random_count = 7;
    lib2->max_random_count = 2;
    lib2->start(sqr);
    counts = count_driven();
    check(lib2->min_random_count == 2 && lib2->sequence_count == 2,
      "a minimum above the maximum is set to the maximum");

    // an empty library and a library without a sequencer do nothing
    other->remove_sequence(seq_b::get_type());
    other->start(sqr);
    check(driven.empty(), "an empty library runs nothing");

    // a library of the base item runs sequences, but has no items to send
    uvm_sequence_library<>* base_lib = new uvm_sequence_library<>("base_lib");
    base_lib->add_sequence(seq_a::get_type());
    base_lib->selection_mode = UVM_SEQ_LIB_ITEM;
    base_lib->min_random_count = 1;
    base_lib->max_random_count = 1;
    base_lib->start(sqr_base);
    check(driven.empty(), "a library of the base item sends no items");
    delete base_lib;

    // sequences added by type at run time
    seq_lib::add_typewide_sequence(seq_d::get_type());
    seq_lib::add_typewide_sequence(seq_d::get_type());
    seq_lib::add_typewide_sequence(plain::get_type());
    seq_lib* lib3 = seq_lib::type_id::create("lib3");
    types.clear();
    lib3->get_sequences(types);
    check(types.size() == 3 && types[2] == seq_d::get_type(),
      "a sequence added by type at run time is in the libraries created afterwards");

    uvm_default_printer->knobs.reference = false;
    lib->print();

    std::cout << (errors == 0 ? "TEST PASSED" : "TEST FAILED") << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  run_test("test");
  return 0;
}
