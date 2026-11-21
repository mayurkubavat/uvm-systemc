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

// A 32-bit register on an 8-bit bus takes four bus transactions. A
// transaction order policy on the map reorders them; the register still
// reads back the value which was written.

#include <systemc>
#include <uvm>

#include <algorithm>
#include <iostream>
#include <map>
#include <string>

using namespace uvm;

std::map<unsigned int, unsigned int> bus_memory;

class bus_item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(bus_item);

  unsigned int addr;
  unsigned int data;
  bool read;

  bus_item( const std::string& name = "bus_item" )
  : uvm_sequence_item(name), addr(0), data(0), read(false) {}
};

class bus_driver : public uvm_driver<bus_item>
{
 public:
  UVM_COMPONENT_UTILS(bus_driver);

  bus_driver( uvm_component_name name ) : uvm_driver<bus_item>(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    bus_item req;

    while (true)
    {
      seq_item_port.get_next_item(req);
      sc_core::wait(1, sc_core::SC_NS);

      if (req.read)
        req.data = bus_memory[req.addr];
      else
        bus_memory[req.addr] = req.data;

      std::cout << "  bus " << (req.read ? "read  " : "write ") << std::hex
                << "0x" << req.addr << " data 0x" << req.data << std::dec << std::endl;

      bus_item rsp;
      rsp = req;
      rsp.set_id_info(req);
      seq_item_port.item_done();
      seq_item_port.put_response(rsp);
    }
  }
};

class bus_adapter : public uvm_reg_adapter
{
 public:
  UVM_OBJECT_UTILS(bus_adapter);

  bus_adapter( const std::string& name = "bus_adapter" ) : uvm_reg_adapter(name)
  {
    provides_responses = true;
  }

  virtual uvm_sequence_item* reg2bus( const uvm_reg_bus_op& rw )
  {
    bus_item* item = bus_item::type_id::create("item");
    item->read = (rw.kind == UVM_READ);
    item->addr = rw.addr.to_uint();
    item->data = rw.data.to_uint();
    return item;
  }

  virtual void bus2reg( const uvm_sequence_item* bus_item_, uvm_reg_bus_op& rw )
  {
    const bus_item* item = dynamic_cast<const bus_item*>(bus_item_);
    rw.kind = item->read ? UVM_READ : UVM_WRITE;
    rw.addr = item->addr;
    rw.data = item->data;
    rw.status = UVM_IS_OK;
  }
};

class reg32 : public uvm_reg
{
 public:
  UVM_OBJECT_UTILS(reg32);

  uvm_reg_field* f;

  reg32( const std::string& name = "reg32" ) : uvm_reg(name, 32, UVM_NO_COVERAGE), f(nullptr) {}

  virtual void build()
  {
    f = uvm_reg_field::type_id::create("f");
    f->configure(this, 32, 0, "RW", false, 0, true, false, true);
  }
};

class block : public uvm_reg_block
{
 public:
  UVM_OBJECT_UTILS(block);

  reg32* r;

  block( const std::string& name = "block" ) : uvm_reg_block(name, UVM_NO_COVERAGE), r(nullptr) {}

  virtual void build()
  {
    default_map = create_map("map", 0, 1, UVM_LITTLE_ENDIAN);
    r = reg32::type_id::create("r");
    r->configure(this);
    r->build();
    default_map->add_reg(r, 0x10, "RW");
    lock_model();
  }
};

// the last byte first
class reverse_order : public uvm_reg_transaction_order_policy
{
 public:
  reverse_order() : uvm_reg_transaction_order_policy("reverse_order") {}

  virtual void order( std::vector<uvm_reg_bus_op>& q )
  {
    std::reverse(q.begin(), q.end());
  }
};

// moves the last transaction to another address, which a policy shall not do
class bad_order : public uvm_reg_transaction_order_policy
{
 public:
  bad_order() : uvm_reg_transaction_order_policy("bad_order") {}

  virtual void order( std::vector<uvm_reg_bus_op>& q )
  {
    q.back().addr = 0x20;
  }
};

// counts the errors of the map
class error_catcher : public uvm_report_catcher
{
 public:
  int caught;

  error_catcher() : uvm_report_catcher("error_catcher"), caught(0) {}

  virtual action_e do_catch()
  {
    if (get_severity() == UVM_ERROR && get_id() == "RegMem")
    {
      caught++;
      std::cout << "caught: " << get_message() << std::endl;
      return CAUGHT;
    }
    return THROW;
  }
};

class reg_seq : public uvm_reg_sequence<>
{
 public:
  UVM_OBJECT_UTILS(reg_seq);

  block* model;
  int errors;

  reg_seq( const std::string& name = "reg_seq" ) : uvm_reg_sequence<>(name), model(nullptr), errors(0) {}

  void write_and_read( unsigned int value )
  {
    uvm_status_e status;
    uvm_reg_data_t data;

    std::cout << "write 0x" << std::hex << value << std::dec << std::endl;
    model->r->write(status, value, UVM_FRONTDOOR, nullptr, this);

    std::cout << "read" << std::endl;
    model->r->read(status, data, UVM_FRONTDOOR, nullptr, this);

    std::cout << "read back 0x" << std::hex << data.to_uint() << std::dec << std::endl;
    if (data.to_uint() != value || status != UVM_IS_OK)
      errors++;
  }

  virtual void body()
  {
    reverse_order policy;

    std::cout << "without a policy" << std::endl;
    write_and_read(0x11223344);

    model->default_map->set_transaction_order_policy(&policy);
    std::cout << "policy set: " << (model->default_map->get_transaction_order_policy() == &policy)
              << std::endl;
    write_and_read(0xa1b2c3d4);

    // a read which the policy moved to another address is reported, and
    // its data is not used
    bad_order bad;
    error_catcher catcher;
    uvm_report_cb::add(nullptr, &catcher);
    model->default_map->set_transaction_order_policy(&bad);
    uvm_status_e status;
    uvm_reg_data_t data;
    model->r->read(status, data, UVM_FRONTDOOR, nullptr, this);
    std::cout << "read with a bad policy: 0x" << std::hex << data.to_uint() << std::dec
              << ", errors " << catcher.caught << std::endl;
    if (catcher.caught != 1 || data.to_uint() != 0x00b2c3d4)
      errors++;
    uvm_report_cb::do_delete(nullptr, &catcher);

    model->default_map->set_transaction_order_policy(nullptr);
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  block* model;
  uvm_sequencer<bus_item>* sqr;
  bus_driver* drv;
  bus_adapter* adapter;

  test( uvm_component_name name )
  : uvm_test(name), model(nullptr), sqr(nullptr), drv(nullptr), adapter(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    model = block::type_id::create("model");
    model->build();
    sqr = new uvm_sequencer<bus_item>("sqr");
    drv = bus_driver::type_id::create("drv", this);
    adapter = bus_adapter::type_id::create("adapter");
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    model->default_map->set_sequencer(sqr, adapter);
    drv->seq_item_port.connect(sqr->seq_item_export);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    reg_seq* seq = reg_seq::type_id::create("seq");
    seq->model = model;
    seq->start(nullptr);

    std::cout << (seq->errors == 0 ? "TEST PASSED" : "TEST FAILED") << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  run_test("test");
  return 0;
}
