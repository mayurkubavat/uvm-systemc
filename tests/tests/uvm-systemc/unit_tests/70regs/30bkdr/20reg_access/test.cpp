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

// Backdoor access to registers: poke, peek, write, read and mirror
// through the HDL paths of a register, for a register with a single
// path, a register made of two slices and a register with two copies in
// the design. The built-in sequence uvm_reg_mem_hdl_paths_seq checks the
// same paths.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

#include "reg.h"

using namespace uvm;

// The design: registers which can be reached by their name
class dut : public sc_core::sc_module
{
 public:
  reg<uvm_reg_data_t> ctrl;
  reg<uvm_reg_data_t> stat_lo;
  reg<uvm_reg_data_t> stat_hi;
  reg<uvm_reg_data_t> id_a;
  reg<uvm_reg_data_t> id_b;

  dut( sc_core::sc_module_name name )
  : sc_core::sc_module(name),
    ctrl("ctrl"), stat_lo("stat_lo"), stat_hi("stat_hi"), id_a("id_a"), id_b("id_b")
  {
    ctrl = 0;
    stat_lo = 0;
    stat_hi = 0;
    id_a = 0;
    id_b = 0;
  }
};

class my_reg : public uvm_reg
{
 public:
  uvm_reg_field* data;

  my_reg( const std::string& name = "my_reg" ) : uvm_reg(name, 16, UVM_NO_COVERAGE)
  {}

  virtual void build()
  {
    data = uvm_reg_field::type_id::create("data");
    data->configure(this, 16, 0, "RW", false, 0x0, true, false, true);
  }

  UVM_OBJECT_UTILS(my_reg);
};

class my_blk : public uvm_reg_block
{
 public:
  my_reg* ctrl;
  my_reg* stat;
  my_reg* id;
  my_reg* nopath;
  my_reg* wide;

  my_blk( const std::string& name = "my_blk" ) : uvm_reg_block(name, UVM_NO_COVERAGE)
  {}

  my_reg* create_reg( const std::string& name, const std::string& hdl_path,
                      uvm_reg_addr_t offset )
  {
    my_reg* r = my_reg::type_id::create(name);
    r->configure(this, nullptr, hdl_path);
    r->build();
    default_map->add_reg(r, offset, "RW");
    return r;
  }

  virtual void build()
  {
    default_map = create_map("default_map", 0x0, 2, UVM_LITTLE_ENDIAN);

    // one path for the whole register
    ctrl = create_reg("ctrl", "ctrl", 0x0);

    // two slices of 8 bits
    stat = create_reg("stat", "", 0x2);
    stat->add_hdl_path_slice("stat_lo", 0, 8);
    stat->add_hdl_path_slice("stat_hi", 8, 8);

    // two copies of the register in the design
    id = create_reg("id", "", 0x4);
    id->add_hdl_path_slice("id_a", -1, -1, true);
    id->add_hdl_path_slice("id_b", -1, -1, true);

    nopath = create_reg("nopath", "", 0x6);

    // a slice which does not fit into the 16 bits of the register
    wide = create_reg("wide", "", 0x8);
    wide->add_hdl_path_slice("ctrl", 12, 8);

    set_hdl_path_root("top");
    lock_model();
  }

  UVM_OBJECT_UTILS(my_blk);
};

class test : public uvm_test
{
 public:
  my_blk* model;
  dut* top;

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name), model(nullptr), top(nullptr)
  {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    if (!uvm_config_db<dut*>::get(this, "", "dut", top))
      UVM_FATAL("TEST", "The design was not found in the configuration database");

    model = my_blk::type_id::create("model");
    model->build();
    model->reset();
  }

  void show( const std::string& what, uvm_status_e status, uvm_reg_data_t value )
  {
    std::cout << what << ": status "
              << ((status == UVM_IS_OK) ? "UVM_IS_OK" : "UVM_NOT_OK")
              << ", value 0x" << std::hex << value.to_uint64() << std::dec << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    uvm_status_e status;
    uvm_reg_data_t value;

    phase.raise_objection(this);

    // a single path
    model->ctrl->poke(status, 0xa5a5);
    show("poke ctrl, design has", status, top->ctrl.read());

    top->ctrl = 0x1234;
    value = 0;
    model->ctrl->peek(status, value);
    show("peek ctrl", status, value);
    show("mirror of ctrl", status, model->ctrl->get_mirrored_value());

    model->ctrl->write(status, 0x0ff0, UVM_BACKDOOR);
    show("backdoor write ctrl, design has", status, top->ctrl.read());

    top->ctrl = 0x4321;
    value = 0;
    model->ctrl->read(status, value, UVM_BACKDOOR);
    show("backdoor read ctrl", status, value);

    // two slices
    model->stat->poke(status, 0xbeef);
    show("poke stat, design has stat_lo", status, top->stat_lo.read());
    show("poke stat, design has stat_hi", status, top->stat_hi.read());

    top->stat_lo = 0x34;
    top->stat_hi = 0x12;
    value = 0;
    model->stat->peek(status, value);
    show("peek stat", status, value);

    // two copies
    model->id->poke(status, 0x00c0);
    show("poke id, design has id_a", status, top->id_a.read());
    show("poke id, design has id_b", status, top->id_b.read());

    value = 0;
    model->id->peek(status, value);
    show("peek id", status, value);

    std::cout << "the copies differ, an error is expected" << std::endl;
    top->id_b = 0x00c1;
    value = 0;
    model->id->peek(status, value);
    show("peek id", status, value);

    // the mirror differs from the design
    std::cout << "the mirror differs from the design, an error is expected" << std::endl;
    model->ctrl->predict(0xabcd);
    model->ctrl->mirror(status, UVM_CHECK, UVM_BACKDOOR);
    show("mirror ctrl with check, mirror is", status, model->ctrl->get_mirrored_value());

    // no path
    std::cout << "the register has no HDL path, errors are expected" << std::endl;
    value = 0;
    model->nopath->peek(status, value);
    show("peek nopath", status, value);
    model->nopath->poke(status, 0x1);
    show("poke nopath", status, 0);

    // a slice outside of the register
    std::cout << "the slice does not fit into the register, errors are expected" << std::endl;
    value = 0;
    model->wide->peek(status, value);
    show("peek wide", status, value);
    model->wide->poke(status, 0xffff);
    show("poke wide, design has ctrl", status, top->ctrl.read());

    // the built-in sequence finds all paths readable
    std::cout << "no further errors are expected" << std::endl;
    uvm_reg_mem_hdl_paths_seq* seq = uvm_reg_mem_hdl_paths_seq::type_id::create("seq");
    seq->model = model;
    seq->start(nullptr);

    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    uvm_coreservice_t* cs = uvm_coreservice_t::get();
    uvm_report_server* svr = cs->get_report_server();

    std::cout << "number of errors: " << svr->get_severity_count(UVM_ERROR) << std::endl;
  }
};

int sc_main(int, char*[])
{
  dut top("top");

  uvm_config_db<dut*>::set(nullptr, "*", "dut", &top);

  run_test("test");

  return 0;
}
