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

// Automatic FC4SC register-layer functional coverage (UVM_CVR_ADDR_MAP,
// UVM_CVR_REG_BITS and UVM_CVR_FIELD_VALS) via uvm_fc4sc_reg_cvr.h.

#include <fstream>
#include <iostream>
#include <map>

#include <systemc>
#include <uvm>
#include <uvmsc/reg/uvm_fc4sc_reg_cvr.h>

using namespace uvm;

struct reg_bus_tx : public uvm_sequence_item {
  bool is_read{false};
  uvm_reg_addr_t addr{0};
  uvm_reg_data_t data{0};

  UVM_OBJECT_UTILS(reg_bus_tx);

  reg_bus_tx(const std::string& name = "reg_bus_tx") : uvm_sequence_item(name) {}
};

class reg_bus_adapter : public uvm_reg_adapter {
public:
  UVM_OBJECT_UTILS(reg_bus_adapter);

  reg_bus_adapter(const std::string& name = "reg_bus_adapter")
      : uvm_reg_adapter(name) {
    provides_responses = true;
  }

  virtual uvm_sequence_item* reg2bus(const uvm_reg_bus_op& rw) {
    reg_bus_tx* tx = reg_bus_tx::type_id::create("tx");
    tx->is_read = (rw.kind == UVM_READ);
    tx->addr = rw.addr;
    tx->data = rw.data;
    return tx;
  }

  virtual void bus2reg(const uvm_sequence_item* bus_item, uvm_reg_bus_op& rw) {
    const reg_bus_tx* tx = dynamic_cast<const reg_bus_tx*>(bus_item);
    if (!tx) UVM_FATAL("ADAPT", "bad item");
    rw.kind = tx->is_read ? UVM_READ : UVM_WRITE;
    rw.addr = tx->addr;
    rw.data = tx->data;
    rw.status = UVM_IS_OK;
  }
};

class reg_bus_driver : public uvm_driver<reg_bus_tx> {
public:
  UVM_COMPONENT_UTILS(reg_bus_driver);

  std::map<uvm_reg_addr_t, uint64_t> hw;

  reg_bus_driver(uvm_component_name name) : uvm_driver<reg_bus_tx>(name) {}

  void run_phase(uvm_phase&) {
    reg_bus_tx req;
    while (true) {
      seq_item_port.get_next_item(req);
      reg_bus_tx rsp("rsp");
      rsp.set_id_info(req);
      rsp.is_read = req.is_read;
      rsp.addr = req.addr;
      if (req.is_read) {
        rsp.data = hw[req.addr];
      } else {
        hw[req.addr] = req.data.to_uint64();
        rsp.data = req.data;
      }
      seq_item_port.item_done(rsp);
    }
  }
};

class ctrl_reg : public uvm_fc4sc_reg {
public:
  uvm_reg_field* en{nullptr};
  uvm_reg_field* mode{nullptr};

  UVM_OBJECT_UTILS(ctrl_reg);

  ctrl_reg(const std::string& name = "ctrl_reg") : uvm_fc4sc_reg(name, 8) {}

  void build() {
    en = uvm_reg_field::type_id::create("en");
    en->configure(this, 1, 0, "RW", false, 0, true, true, false);
    mode = uvm_reg_field::type_id::create("mode");
    mode->configure(this, 2, 1, "RW", false, 0, true, true, false);
  }
};

class status_reg : public uvm_fc4sc_reg {
public:
  uvm_reg_field* ready{nullptr};

  UVM_OBJECT_UTILS(status_reg);

  status_reg(const std::string& name = "status_reg") : uvm_fc4sc_reg(name, 8) {}

  void build() {
    ready = uvm_reg_field::type_id::create("ready");
    ready->configure(this, 1, 0, "RO", true, 0, true, false, false);
  }
};

class periph_block : public uvm_fc4sc_reg_block {
public:
  ctrl_reg* ctrl{nullptr};
  status_reg* status{nullptr};
  uvm_mem* buf{nullptr};

  UVM_OBJECT_UTILS(periph_block);

  periph_block(const std::string& name = "blk") : uvm_fc4sc_reg_block(name) {}

  void build() {
    default_map = create_map("default_map", 0, 1, UVM_LITTLE_ENDIAN, true);

    ctrl = new ctrl_reg("ctrl");
    ctrl->configure(this);
    ctrl->build();
    default_map->add_reg(ctrl, 0x0);

    status = new status_reg("status");
    status->configure(this);
    status->build();
    default_map->add_reg(status, 0x4);

    buf = new uvm_mem("buf", 4, 8, "RW", UVM_CVR_ALL);
    buf->configure(this);
    default_map->add_mem(buf, 0x10);

    lock_model();
  }
};

class fc4sc_reg_test : public uvm_test {
public:
  UVM_COMPONENT_UTILS(fc4sc_reg_test);

  periph_block* blk{nullptr};
  uvm_sequencer<reg_bus_tx>* sqr{nullptr};
  reg_bus_driver* drv{nullptr};
  reg_bus_adapter* adapt{nullptr};

  fc4sc_reg_test(uvm_component_name name) : uvm_test(name) {}

  void build_phase(uvm_phase& phase) {
    uvm_test::build_phase(phase);
    uvm_reg::include_coverage("*", UVM_CVR_ALL);

    blk = new periph_block("blk");
    blk->build();
    blk->reset();
    blk->enable_all_coverage(UVM_CVR_ALL);

    sqr = new uvm_sequencer<reg_bus_tx>("sqr");
    drv = reg_bus_driver::type_id::create("drv", this);
    adapt = reg_bus_adapter::type_id::create("adapt");
  }

  void connect_phase(uvm_phase&) {
    drv->seq_item_port.connect(sqr->seq_item_export);
    blk->default_map->set_sequencer(sqr, adapt);
    blk->default_map->set_auto_predict(true);
  }

  void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);

    std::cout << "initial: addr=" << static_cast<int>(blk->get_addr_map_coverage())
              << "% ctrl_bits=" << static_cast<int>(blk->ctrl->get_reg_bits_coverage())
              << "% ctrl_vals=" << static_cast<int>(blk->ctrl->get_field_vals_coverage())
              << "% status_bits=" << static_cast<int>(blk->status->get_reg_bits_coverage())
              << "%" << std::endl;

    uvm_status_e st;
    uvm_reg_data_t rd = 0;

    // Sweep ctrl: en in {0,1}, mode in {0,1,2,3}, writing and reading back
    for (unsigned en = 0; en <= 1; ++en) {
      for (unsigned mode = 0; mode <= 3; ++mode) {
        uvm_reg_data_t v = (mode << 1) | en;
        blk->ctrl->write(st, v);
        blk->ctrl->read(st, rd);
        blk->sample_values();
      }
    }

    // Read RO status register as both 0 and 1
    drv->hw[0x4] = 0;
    blk->status->read(st, rd);
    blk->status->sample_values();
    drv->hw[0x4] = 1;
    blk->status->read(st, rd);
    blk->status->sample_values();

    // Write and read mapped memory buf
    blk->buf->write(st, 0, 0xa5);
    blk->buf->read(st, 0, rd);

    std::cout << "final: addr=" << static_cast<int>(blk->get_addr_map_coverage())
              << "% ctrl_bits=" << static_cast<int>(blk->ctrl->get_reg_bits_coverage())
              << "% ctrl_vals=" << static_cast<int>(blk->ctrl->get_field_vals_coverage())
              << "% status_bits=" << static_cast<int>(blk->status->get_reg_bits_coverage())
              << "%" << std::endl;

    xml_printer::coverage_save("fc4sc_reg_coverage.xml");
    std::ifstream in("fc4sc_reg_coverage.xml");
    std::cout << "ucis_xml_written=" << (in.good() ? 1 : 0) << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  run_test("fc4sc_reg_test");
  return 0;
}
