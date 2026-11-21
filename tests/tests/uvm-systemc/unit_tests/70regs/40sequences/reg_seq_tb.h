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

// Test bench shared by the tests of the built-in register and memory
// test sequences: a register model with two address maps, a bus driver
// and a model of the DUT in which faults can be switched on.

#ifndef REG_SEQ_TB_H_
#define REG_SEQ_TB_H_

#include <systemc>
#include <uvm>

#include <iostream>
#include <map>
#include <string>

using namespace uvm;

//----------------------------------------------------------------------
// DUT model: four registers and a memory behind two bus interfaces.
// Interface B sees the same storage at an offset of 0x1000 and has no
// access to the register r_ro.
//----------------------------------------------------------------------

struct dut_model
{
  enum { R_RW, R_MIX, R_RO, R_WO, N_REGS };
  enum { MEM_SIZE = 8 };

  sc_dt::uint64 regs[N_REGS];
  sc_dt::uint64 mem[MEM_SIZE];

  // faults
  bool bad_reset;      // r_rw has a wrong value after reset
  bool stuck_bit;      // bit 0 of r_rw is stuck at 0
  bool mem_stuck_bit;  // bit 31 of memory word 2 is stuck at 0
  bool mem_drop_bit;   // bit 0 is lost when word 5 is written via the bus
  bool b_write_dead;   // interface B ignores writes

  dut_model()
  : bad_reset(false), stuck_bit(false), mem_stuck_bit(false),
    mem_drop_bit(false), b_write_dead(false)
  {
    reset();
  }

  void clear_faults()
  {
    bad_reset = stuck_bit = mem_stuck_bit = mem_drop_bit = b_write_dead = false;
  }

  void reset()
  {
    regs[R_RW]  = bad_reset ? 0x12345670 : 0x12345678;
    regs[R_MIX] = 0x3CA5;
    regs[R_RO]  = 0xCAFE0001;
    regs[R_WO]  = 0x0000BEEF;
    for (int i = 0; i < MEM_SIZE; i++)
      mem[i] = 0;
  }

  unsigned int mem_index( unsigned int addr ) const
  {
    return (addr - 0x100) / 4;
  }

  sc_dt::uint64 read( unsigned int addr ) const
  {
    unsigned int local = addr & 0xFFF;

    if (local >= 0x100)
      return mem[mem_index(local)];
    if (local / 4 == R_WO) // the upper half is write-only
      return regs[R_WO] & 0xFFFF;
    return regs[local / 4];
  }

  void write( unsigned int addr, sc_dt::uint64 data )
  {
    unsigned int local = addr & 0xFFF;

    if (b_write_dead && addr >= 0x1000)
      return;

    data &= 0xFFFFFFFF;

    if (local >= 0x100)
    {
      unsigned int idx = mem_index(local);
      if (mem_drop_bit && idx == 5)
        data &= ~sc_dt::uint64(1);
      if (mem_stuck_bit && idx == 2)
        data &= ~sc_dt::uint64(0x80000000);
      mem[idx] = data;
      return;
    }

    switch (local / 4)
    {
      case R_RW:
        regs[R_RW] = stuck_bit ? (data & ~sc_dt::uint64(0x1)) : data;
        break;
      case R_MIX: // only the lower byte can be written
        regs[R_MIX] = (regs[R_MIX] & 0xFF00) | (data & 0xFF);
        break;
      case R_WO:
        regs[R_WO] = data;
        break;
      default: // read-only
        break;
    }
  }
};

inline dut_model& dut()
{
  static dut_model d;
  return d;
}

//----------------------------------------------------------------------
// Register model
//----------------------------------------------------------------------

class tb_reg_rw : public uvm_reg
{
 public:
  uvm_reg_field* value;

  UVM_OBJECT_UTILS(tb_reg_rw);

  tb_reg_rw( const std::string& name = "tb_reg_rw" )
  : uvm_reg(name, 32, UVM_NO_COVERAGE), value(nullptr) {}

  void build()
  {
    value = uvm_reg_field::type_id::create("value");
    value->configure(this, 32, 0, "RW", false, 0x12345678, true, false, true);
  }
};

class tb_reg_mix : public uvm_reg
{
 public:
  uvm_reg_field* lo;
  uvm_reg_field* hi;

  UVM_OBJECT_UTILS(tb_reg_mix);

  tb_reg_mix( const std::string& name = "tb_reg_mix" )
  : uvm_reg(name, 32, UVM_NO_COVERAGE), lo(nullptr), hi(nullptr) {}

  void build()
  {
    lo = uvm_reg_field::type_id::create("lo");
    lo->configure(this, 8, 0, "RW", false, 0xA5, true, false, false);
    hi = uvm_reg_field::type_id::create("hi");
    hi->configure(this, 8, 8, "RO", false, 0x3C, true, false, false);
  }
};

class tb_reg_ro : public uvm_reg
{
 public:
  uvm_reg_field* value;

  UVM_OBJECT_UTILS(tb_reg_ro);

  tb_reg_ro( const std::string& name = "tb_reg_ro" )
  : uvm_reg(name, 32, UVM_NO_COVERAGE), value(nullptr) {}

  void build()
  {
    value = uvm_reg_field::type_id::create("value");
    value->configure(this, 32, 0, "RO", false, 0xCAFE0001, true, false, true);
  }
};

class tb_reg_wo : public uvm_reg
{
 public:
  uvm_reg_field* lo;
  uvm_reg_field* hi;

  UVM_OBJECT_UTILS(tb_reg_wo);

  tb_reg_wo( const std::string& name = "tb_reg_wo" )
  : uvm_reg(name, 32, UVM_NO_COVERAGE), lo(nullptr), hi(nullptr) {}

  void build()
  {
    lo = uvm_reg_field::type_id::create("lo");
    lo->configure(this, 16, 0, "RW", false, 0xBEEF, true, false, false);
    hi = uvm_reg_field::type_id::create("hi");
    hi->configure(this, 16, 16, "WO", false, 0x0, true, false, false);
  }
};

class dut_block : public uvm_reg_block
{
 public:
  tb_reg_rw*  r_rw;
  tb_reg_mix* r_mix;
  tb_reg_ro*  r_ro;
  tb_reg_wo*  r_wo;
  uvm_mem* mem;
  uvm_reg_map* map_a;
  uvm_reg_map* map_b;

  UVM_OBJECT_UTILS(dut_block);

  dut_block( const std::string& name = "dut_block" )
  : uvm_reg_block(name, UVM_NO_COVERAGE),
    r_rw(nullptr), r_mix(nullptr), r_ro(nullptr), r_wo(nullptr), mem(nullptr),
    map_a(nullptr), map_b(nullptr) {}

  void build()
  {
    r_rw = tb_reg_rw::type_id::create("r_rw");
    r_rw->configure(this);
    r_rw->build();

    r_mix = tb_reg_mix::type_id::create("r_mix");
    r_mix->configure(this);
    r_mix->build();

    r_ro = tb_reg_ro::type_id::create("r_ro");
    r_ro->configure(this);
    r_ro->build();

    r_wo = tb_reg_wo::type_id::create("r_wo");
    r_wo->configure(this);
    r_wo->build();

    mem = new uvm_mem("mem", dut_model::MEM_SIZE, 32, "RW", UVM_NO_COVERAGE);
    mem->configure(this);

    map_a = create_map("map_a", 0x0, 4, UVM_LITTLE_ENDIAN);
    map_a->add_reg(r_rw,  0x0, "RW");
    map_a->add_reg(r_mix, 0x4, "RW");
    map_a->add_reg(r_ro,  0x8, "RO");
    map_a->add_reg(r_wo,  0xC, "RW");
    map_a->add_mem(mem, 0x100, "RW");

    map_b = create_map("map_b", 0x1000, 4, UVM_LITTLE_ENDIAN);
    map_b->add_reg(r_rw,  0x0, "RW");
    map_b->add_reg(r_mix, 0x4, "RW");
    map_b->add_reg(r_wo,  0xC, "RW");
    map_b->add_mem(mem, 0x100, "RW");

    set_default_map(map_a);
    lock_model();
  }
};

//----------------------------------------------------------------------
// Backdoor: direct access to the storage of the DUT model
//----------------------------------------------------------------------

class dut_backdoor : public uvm_reg_backdoor
{
 public:
  dut_block* model;

  dut_backdoor( const std::string& name = "dut_backdoor" )
  : uvm_reg_backdoor(name), model(nullptr) {}

  sc_dt::uint64* storage( uvm_reg_item* rw )
  {
    if (rw->element == model->mem)   return &dut().mem[rw->offset.to_uint()];
    if (rw->element == model->r_rw)  return &dut().regs[dut_model::R_RW];
    if (rw->element == model->r_mix) return &dut().regs[dut_model::R_MIX];
    if (rw->element == model->r_ro)  return &dut().regs[dut_model::R_RO];
    if (rw->element == model->r_wo)  return &dut().regs[dut_model::R_WO];
    return nullptr;
  }

  virtual void read_func( uvm_reg_item* rw )
  {
    sc_dt::uint64* s = storage(rw);
    rw->status = (s != nullptr) ? UVM_IS_OK : UVM_NOT_OK;
    if (s != nullptr)
      rw->value[0] = *s;
  }

  virtual void write( uvm_reg_item* rw )
  {
    sc_dt::uint64* s = storage(rw);
    rw->status = (s != nullptr) ? UVM_IS_OK : UVM_NOT_OK;
    if (s != nullptr)
      *s = rw->value[0].to_uint64();
  }
};

//----------------------------------------------------------------------
// Bus agent
//----------------------------------------------------------------------

class bus_item : public uvm_sequence_item
{
 public:
  unsigned int addr;
  sc_dt::uint64 data;
  bool is_read;

  UVM_OBJECT_UTILS(bus_item);

  bus_item( const std::string& name = "bus_item" )
  : uvm_sequence_item(name), addr(0), data(0), is_read(false) {}
};

class bus_driver : public uvm_driver<bus_item>
{
 public:
  UVM_COMPONENT_UTILS(bus_driver);

  bus_driver( uvm_component_name name ) : uvm_driver<bus_item>(name) {}

  void run_phase( uvm_phase& phase )
  {
    while (true)
    {
      bus_item req, rsp;

      seq_item_port.get_next_item(req);
      sc_core::wait(10, sc_core::SC_NS);

      if (req.is_read)
        req.data = dut().read(req.addr);
      else
        dut().write(req.addr, req.data);

      // the read data is returned in a response
      rsp.addr = req.addr;
      rsp.data = req.data;
      rsp.is_read = req.is_read;
      rsp.set_id_info(req);
      seq_item_port.item_done(rsp);
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
    item->is_read = (rw.kind == UVM_READ);
    item->addr = rw.addr.to_uint();
    item->data = rw.data.to_uint64();
    return item;
  }

  virtual void bus2reg( const uvm_sequence_item* bus, uvm_reg_bus_op& rw )
  {
    const bus_item* item = dynamic_cast<const bus_item*>(bus);

    if (item == nullptr)
      UVM_FATAL("TB", "bus item of wrong type");

    rw.kind = item->is_read ? UVM_READ : UVM_WRITE;
    rw.addr = item->addr;
    rw.data = item->data;
    rw.status = UVM_IS_OK;
  }
};

//----------------------------------------------------------------------
// Built-in sequences which reset the DUT model. The built-in sequences
// leave the reset of the DUT to the member function reset_blk.
//----------------------------------------------------------------------

#define TB_SEQ_WITH_DUT_RESET(NAME, BASE)                              \
  class NAME : public BASE                                             \
  {                                                                    \
   public:                                                             \
    UVM_OBJECT_UTILS(NAME);                                            \
    NAME( const std::string& name = #NAME ) : BASE(name) {}            \
    virtual void reset_blk( uvm_reg_block* blk ) { dut().reset(); }    \
  }

TB_SEQ_WITH_DUT_RESET(tb_hw_reset_seq,      uvm_reg_hw_reset_seq);
TB_SEQ_WITH_DUT_RESET(tb_bit_bash_seq,      uvm_reg_bit_bash_seq);
TB_SEQ_WITH_DUT_RESET(tb_reg_access_seq,    uvm_reg_access_seq);
TB_SEQ_WITH_DUT_RESET(tb_mem_access_seq,    uvm_mem_access_seq);
TB_SEQ_WITH_DUT_RESET(tb_shared_access_seq, uvm_reg_mem_shared_access_seq);
TB_SEQ_WITH_DUT_RESET(tb_mem_walk_seq,      uvm_mem_walk_seq);

//----------------------------------------------------------------------
// Base test: builds the bench and counts the errors of a sequence
//----------------------------------------------------------------------

class tb_test : public uvm_test
{
 public:
  dut_block* model;
  dut_backdoor* backdoor;
  bus_adapter* adapter;
  uvm_sequencer<bus_item>* sqr;
  bus_driver* drv;

  tb_test( uvm_component_name name )
  : uvm_test(name), model(nullptr), backdoor(nullptr), adapter(nullptr),
    sqr(nullptr), drv(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    model = dut_block::type_id::create("model");
    model->build();

    backdoor = new dut_backdoor();
    backdoor->model = model;
    model->set_backdoor(backdoor);

    sqr = new uvm_sequencer<bus_item>("sqr");
    drv = bus_driver::type_id::create("drv", this);

    // the sequences started by another built-in sequence reset the DUT too
    uvm_reg_hw_reset_seq::type_id::set_type_override(tb_hw_reset_seq::get_type());
    uvm_reg_bit_bash_seq::type_id::set_type_override(tb_bit_bash_seq::get_type());
    uvm_reg_access_seq::type_id::set_type_override(tb_reg_access_seq::get_type());
    uvm_mem_access_seq::type_id::set_type_override(tb_mem_access_seq::get_type());
    uvm_reg_mem_shared_access_seq::type_id::set_type_override(tb_shared_access_seq::get_type());
    uvm_mem_walk_seq::type_id::set_type_override(tb_mem_walk_seq::get_type());
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    adapter = new bus_adapter();
    model->map_a->set_sequencer(sqr, adapter);
    model->map_b->set_sequencer(sqr, adapter);
    model->map_a->set_auto_predict(true);
    model->map_b->set_auto_predict(true);
    drv->seq_item_port.connect(sqr->seq_item_export);
  }

  static int errors()
  {
    return uvm_report_server::get_server()->get_severity_count(UVM_ERROR);
  }

  // Run the sequence SEQ on the model and return its number of errors
  template <typename SEQ>
  int run_seq( sc_dt::uint64 tests = 0 )
  {
    int before = errors();
    SEQ* seq = SEQ::type_id::create("seq");

    seq->model = model;
    set_tests(seq, tests);
    seq->start(nullptr);
    SEQ::type_id::destroy(seq);

    return errors() - before;
  }

 private:
  void set_tests( uvm_reg_mem_built_in_seq* seq, sc_dt::uint64 tests )
  {
    if (tests != 0)
      seq->tests = tests;
  }

  void set_tests( uvm_sequence_base* seq, sc_dt::uint64 tests ) {}
};

#endif // REG_SEQ_TB_H_
