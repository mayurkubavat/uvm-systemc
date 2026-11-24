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

// Transaction recording: a component records transactions, child
// transactions, events and errors into the text database, and a
// transaction records itself on a stream. The test prints the database
// file, with its handles replaced by names, as they depend on the order
// in which objects are created.

#include <systemc>
#include <uvm>

#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>

using namespace uvm;

class packet : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(packet);

  int addr;
  int data;
  packet* inner; // recorded as an object inside the packet

  packet( const std::string& name = "packet" )
  : uvm_sequence_item(name), addr(0), data(0), inner(nullptr) {}

  virtual void do_record( const uvm_recorder& recorder )
  {
    uvm_sequence_item::do_record(recorder);
    uvm_recorder* r = const_cast<uvm_recorder*>(&recorder);
    r->record_field_int("addr", addr, 32, UVM_HEX);
    r->record_field("data", data, 16, UVM_DEC);
    r->record_string("kind", "write");
    if (inner != nullptr)
      r->record_object("inner", inner);
  }
};

class monitor : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(monitor);

  monitor( uvm_component_name name ) : uvm_component(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    packet burst("burst");
    packet beat("beat");
    packet nested("nested");
    burst.addr = 0x40; burst.data = 7; burst.inner = &nested;
    beat.addr = 0x44; beat.data = 8;

    sc_core::wait(10, sc_core::SC_NS);
    int burst_h = begin_tr(burst, "bus", "burst", "a burst of one beat");
    std::cout << "recorder of the burst found by its handle: "
              << (uvm_recorder::get_recorder_from_handle(burst_h) != nullptr) << std::endl;

    sc_core::wait(5, sc_core::SC_NS);
    begin_child_tr(beat, burst_h, "bus");
    sc_core::wait(5, sc_core::SC_NS);
    end_tr(beat);

    record_event_tr("bus", nullptr, "idle", "the bus is idle");
    record_error_tr("bus", &beat, "parity", "a parity error");

    sc_core::wait(5, sc_core::SC_NS);
    end_tr(burst);
    std::cout << "no recorder for the handle after the end: "
              << (uvm_recorder::get_recorder_from_handle(burst_h) == nullptr) << std::endl;

    // a transaction which records itself on a stream
    uvm_tr_stream* stream = get_tr_stream("self", "TVM");
    packet own("own");
    own.addr = 0x80;
    own.enable_recording(stream);
    sc_core::wait(5, sc_core::SC_NS);
    int own_h = own.begin_tr();
    std::cout << "the transaction is active: " << own.is_active()
              << ", its handle: " << (own.get_tr_handle() == own_h && own_h != 0) << std::endl;
    sc_core::wait(5, sc_core::SC_NS);
    own.end_tr(sc_core::SC_ZERO_TIME, false);
    std::cout << "a recorder which is not freed stays: "
              << (uvm_recorder::get_recorder_from_handle(own_h) != nullptr) << std::endl;

    std::vector<uvm_recorder*> recorders;
    stream->get_recorders(recorders);
    std::cout << "recorders in the stream: " << recorders.size() << std::endl;

    free_tr_stream(stream);
    std::cout << "the stream frees its recorders: "
              << (uvm_recorder::get_recorder_from_handle(own_h) == nullptr) << std::endl;

    // a transaction whose stream is freed before it ends
    packet early("early");
    begin_tr(early, "short");
    free_tr_stream(get_tr_stream("short", "TVM"));
    end_tr(early);

    uvm_tr_stream* short_lived = get_tr_stream("short_self", "TVM");
    early.enable_recording(short_lived);
    early.begin_tr();
    short_lived->free();
    early.end_tr();
    std::cout << "ending transactions after their stream was freed is harmless" << std::endl;

    phase.drop_objection(this);
  }
};

// recording is off: nothing is recorded
class quiet : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(quiet);

  quiet( uvm_component_name name ) : uvm_component(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    packet p("unrecorded");
    std::cout << "handle without recording: " << begin_tr(p) << std::endl;
    end_tr(p);
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  monitor* mon;
  quiet* q;

  test( uvm_component_name name ) : uvm_test(name), mon(nullptr), q(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    uvm_config_db<int>::set(this, "mon", "recording_detail", UVM_LOW);
    mon = monitor::type_id::create("mon", this);
    q = quiet::type_id::create("q", this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    std::cout << "recording enabled: mon " << mon->get_recording_enabled()
              << ", q " << q->get_recording_enabled() << std::endl;
  }
};

// prints the database file with its numbers replaced by names
void print_database( const std::string& filename )
{
  std::ifstream in(filename.c_str());
  std::map<std::string, std::string> names;
  std::regex handle("(TXH1?:|TXH2:|STREAM:)([0-9]+)");
  std::regex inst_id("(NAME:inst_id VALUE:)([0-9]+)");
  std::string line;

  while (std::getline(in, line))
  {
    std::string out;
    std::smatch m;
    std::string rest = line;

    while (std::regex_search(rest, m, handle))
    {
      std::string& name = names[m[2].str()];
      if (name.empty())
      {
        std::ostringstream str;
        str << "h" << names.size();
        name = str.str();
      }
      out += m.prefix().str() + m[1].str() + name;
      rest = m.suffix().str();
    }
    out += rest;
    std::cout << std::regex_replace(out, inst_id, "$1<id>") << std::endl;
  }
}

int sc_main( int, char*[] )
{
  const std::string filename = "recording_test_db.log";
  std::remove(filename.c_str());

  uvm_text_tr_database* db = dynamic_cast<uvm_text_tr_database*>(
    uvm_coreservice_t::get()->get_default_tr_database());
  db->set_file_name(filename);

  run_test("test");

  db->close_db();
  std::cout << "database closed: " << !db->is_open() << std::endl;
  print_database(filename);
  return 0;
}
