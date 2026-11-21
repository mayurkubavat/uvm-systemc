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

// Regression test for:
//   1. uvm_reg_fifo::do_predict() (push on write, pop & check on read,
//      no duplicate push during set()+update(), empty-safe get() and reset())
//   2. uvm_mem::get_vreg_by_offset() (memory-offset and map-offset lookup)
//   3. uvm_reg_field::do_copy() and uvm_reg_field::do_compare()

#include <cstdint>
#include <deque>
#include <iostream>

#include <systemc>
#include <uvm>

using namespace uvm;

class bus_item : public uvm_sequence_item {
 public:
  uint64_t addr = 0;
  uint64_t data = 0;
  bool write = false;

  UVM_OBJECT_UTILS(bus_item);

  explicit bus_item(const std::string& name = "bus_item")
      : uvm_sequence_item(name) {}
};

class bus_adapter : public uvm_reg_adapter {
 public:
  UVM_OBJECT_UTILS(bus_adapter);

  explicit bus_adapter(const std::string& name = "bus_adapter")
      : uvm_reg_adapter(name) {
    provides_responses = true;
  }

  virtual uvm_sequence_item* reg2bus(const uvm_reg_bus_op& rw) {
    auto* tr = bus_item::type_id::create("tr");
    tr->addr = rw.addr.to_uint64();
    tr->data = rw.data.to_uint64();
    tr->write = (rw.kind == UVM_WRITE);
    return tr;
  }

  virtual void bus2reg(const uvm_sequence_item* bus_tr, uvm_reg_bus_op& rw) {
    const auto* tr = dynamic_cast<const bus_item*>(bus_tr);
    rw.addr = tr->addr;
    rw.data = tr->data;
    rw.kind = tr->write ? UVM_WRITE : UVM_READ;
    rw.status = UVM_IS_OK;
  }
};

class fifo_driver : public uvm_driver<bus_item> {
 public:
  UVM_COMPONENT_UTILS(fifo_driver);

  uvm_analysis_port<bus_item> ap{"ap"};
  std::deque<uint64_t> hw_fifo;

  explicit fifo_driver(uvm_component_name name) : uvm_driver<bus_item>(name) {}

  virtual void run_phase(uvm_phase&) {
    while (true) {
      bus_item req;
      sc_core::wait(1, sc_core::SC_NS);
      seq_item_port->get_next_item(req);
      sc_core::wait(1, sc_core::SC_NS);
      if (req.addr == 0x0) {
        if (req.write) {
          hw_fifo.push_back(req.data & 0xFFu);
        } else {
          req.data = hw_fifo.empty() ? 0u : hw_fifo.front();
          if (!hw_fifo.empty()) hw_fifo.pop_front();
        }
      }
      ap.write(req);
      bus_item rsp;
      rsp.addr = req.addr;
      rsp.data = req.data;
      rsp.write = req.write;
      rsp.set_id_info(req);
      seq_item_port->item_done(rsp);
    }
  }
};

class my_reg : public uvm_reg {
 public:
  uvm_reg_field* f = nullptr;
  UVM_OBJECT_UTILS(my_reg);

  explicit my_reg(const std::string& name = "my_reg")
      : uvm_reg(name, 32, UVM_NO_COVERAGE) {}

  void build() {
    f = uvm_reg_field::type_id::create("f");
    f->configure(this, 8, 0, "RW", false, 0x0, true, false, true);
  }
};

class my_vreg : public uvm_vreg {
 public:
  uvm_vreg_field* vf = nullptr;
  UVM_OBJECT_UTILS(my_vreg);

  explicit my_vreg(const std::string& name = "my_vreg")
      : uvm_vreg(name, 32) {}

  void build() {
    vf = uvm_vreg_field::type_id::create("vf");
    vf->configure(this, 32, 0);
  }
};

class my_blk : public uvm_reg_block {
 public:
  uvm_reg_fifo* fifo = nullptr;
  my_reg* r0 = nullptr;
  my_reg* r1 = nullptr;
  uvm_mem* mem = nullptr;
  my_vreg* vr_a = nullptr;
  my_vreg* vr_b = nullptr;

  UVM_OBJECT_UTILS(my_blk);

  explicit my_blk(const std::string& name = "my_blk")
      : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  void build() {
    default_map = create_map("default_map", 0x0, 4, UVM_LITTLE_ENDIAN);

    fifo = new uvm_reg_fifo("fifo", 4, 32, UVM_NO_COVERAGE);
    fifo->configure(this, nullptr);
    fifo->build();
    fifo->set_compare(UVM_CHECK);
    default_map->add_reg(fifo, 0x00, "RW");

    r0 = my_reg::type_id::create("r0");
    r0->configure(this, nullptr);
    r0->build();
    default_map->add_reg(r0, 0x04, "RW");

    r1 = my_reg::type_id::create("r1");
    r1->configure(this, nullptr);
    r1->build();
    default_map->add_reg(r1, 0x08, "RW");

    mem = new uvm_mem("mem", 32, 32, "RW", UVM_NO_COVERAGE);
    mem->configure(this);
    default_map->add_mem(mem, 0x100, "RW");

    vr_a = my_vreg::type_id::create("vr_a");
    vr_a->configure(this, mem, 4, 2, 2);  // 4 entries at mem offsets 2, 4, 6, 8
    vr_a->build();

    vr_b = my_vreg::type_id::create("vr_b");
    vr_b->configure(this, mem, 2, 16, 1); // 2 entries at mem offsets 16, 17
    vr_b->build();

    lock_model();
  }
};

class test : public uvm_test {
 public:
  UVM_COMPONENT_UTILS(test);

  my_blk* model = nullptr;
  uvm_sequencer<bus_item>* sqr = nullptr;
  fifo_driver* drv = nullptr;
  bus_adapter* adapter = nullptr;
  uvm_reg_predictor<bus_item>* pred = nullptr;

  explicit test(uvm_component_name name) : uvm_test(name) {}

  virtual void build_phase(uvm_phase& phase) {
    uvm_test::build_phase(phase);
    model = my_blk::type_id::create("model");
    model->build();
    model->reset();

    sqr = new uvm_sequencer<bus_item>("sqr");
    drv = fifo_driver::type_id::create("drv", this);
    adapter = bus_adapter::type_id::create("adapter");
    pred = new uvm_reg_predictor<bus_item>("pred");
  }

  virtual void connect_phase(uvm_phase& phase) {
    uvm_test::connect_phase(phase);
    drv->seq_item_port.connect(sqr->seq_item_export);
    model->default_map->set_sequencer(sqr, adapter);
    model->default_map->set_auto_predict(false);
    pred->map = model->default_map;
    pred->adapter = adapter;
    drv->ap.connect(pred->bus_in);
  }

  virtual void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);

    // 1. Empty FIFO get() must return 0 safely
    std::cout << "empty_get=" << model->fifo->get().to_uint64()
              << " size=" << model->fifo->size() << std::endl;

    // 2. set() + update() (partial FIFO: 2 of 4) must not duplicate via predictor
    uvm_status_e status = UVM_NOT_OK;
    uvm_reg_data_t rd = 0;
    model->fifo->set(0x11);
    model->fifo->set(0x22);
    model->fifo->update(status);
    std::cout << "after_update_size=" << model->fifo->size() << std::endl;

    // 3. Direct write() pushes via predictor
    model->fifo->write(status, 0x33);
    std::cout << "after_write_size=" << model->fifo->size()
              << " front=0x" << std::hex << model->fifo->get().to_uint64() << std::dec << std::endl;

    // 4. Three reads pop in FIFO order and compare cleanly against mirror
    for (int i = 0; i < 3; ++i) {
      model->fifo->read(status, rd);
      std::cout << "pop" << i << "=0x" << std::hex << rd.to_uint64()
                << std::dec << " rem=" << model->fifo->size() << std::endl;
    }

    // 5. uvm_mem::get_vreg_by_offset (memory word offsets & map byte offsets)
    uvm_vreg* hit_mem4 = model->mem->get_vreg_by_offset(4);
    uvm_vreg* hit_mem3 = model->mem->get_vreg_by_offset(3);  // between stride=2 slots!
    uvm_vreg* hit_map = model->mem->get_vreg_by_offset(0x100 + 17 * 4, model->default_map);
    std::cout << "vreg_mem4=" << (hit_mem4 ? hit_mem4->get_name() : "null")
              << " vreg_mem3=" << (hit_mem3 ? hit_mem3->get_name() : "null")
              << " vreg_map17=" << (hit_map ? hit_map->get_name() : "null") << std::endl;

    // 6. uvm_reg_field::copy() and compare()
    model->r0->f->set(0x5A);
    model->r1->f->copy(*model->r0->f);
    uint64_t copied_val = model->r1->f->get().to_uint64();
    bool eq1 = model->r1->f->compare(*model->r0->f);
    model->r1->f->set(0xA5);
    bool eq2 = model->r1->f->compare(*model->r0->f);
    std::cout << "field_copy=0x" << std::hex << copied_val << std::dec
              << " eq_same=" << eq1 << " eq_diff=" << eq2 << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  run_test("test");
  return 0;
}
