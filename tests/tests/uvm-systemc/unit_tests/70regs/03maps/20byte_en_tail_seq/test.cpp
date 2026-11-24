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

// Reproducing test for Fix 3 (uvm_reg_map):
//   1. Per-beat byte_en not clobbered & UVM_FIELD multi-beat addrs trimming
//   2. Narrow tail beat masking in do_bus_write and do_bus_read (with/without policy)
//   3. tmp_parent_seq cleanup & rw->parent restoration

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>

#include <systemc>
#include <uvm>

using namespace uvm;

struct beat_rec {
  uint64_t addr;
  uint64_t data;
  uint64_t byte_en;
  unsigned int n_bits;
  bool write;
};

class bus_item : public uvm_sequence_item {
 public:
  uint64_t addr = 0;
  uint64_t data = 0;
  uint64_t byte_en = 0;
  unsigned int n_bits = 0;
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
    supports_byte_enable = true;
  }

  virtual uvm_sequence_item* reg2bus(const uvm_reg_bus_op& rw) {
    auto* tr = bus_item::type_id::create("tr");
    tr->addr = rw.addr.to_uint64();
    tr->data = rw.data.to_uint64();
    tr->byte_en = rw.byte_en.to_uint64();
    tr->n_bits = rw.n_bits;
    tr->write = (rw.kind == UVM_WRITE);
    return tr;
  }

  virtual void bus2reg(const uvm_sequence_item* bus_tr, uvm_reg_bus_op& rw) {
    const auto* tr = dynamic_cast<const bus_item*>(bus_tr);
    rw.addr = tr->addr;
    rw.data = tr->data;
    rw.byte_en = tr->byte_en;
    rw.n_bits = tr->n_bits;
    rw.kind = tr->write ? UVM_WRITE : UVM_READ;
    rw.status = UVM_IS_OK;
  }
};

class bus_driver : public uvm_driver<bus_item> {
 public:
  UVM_COMPONENT_UTILS(bus_driver);

  std::vector<beat_rec> log;
  std::map<uint64_t, uint64_t> mem;
  uint64_t tail_garbage_or = 0;
  uint64_t tail_addr = 0x202;

  explicit bus_driver(uvm_component_name name) : uvm_driver<bus_item>(name) {}

  virtual void run_phase(uvm_phase&) {
    while (true) {
      bus_item req;
      seq_item_port->get_next_item(req);
      sc_core::wait(1, sc_core::SC_NS);
      if (req.write) {
        for (unsigned int b = 0; b < 2; ++b) {
          if ((req.byte_en >> b) & 1u) {
            uint64_t mask = 0xFFULL << (b * 8);
            mem[req.addr] = (mem[req.addr] & ~mask) | (req.data & mask);
          }
        }
      } else {
        req.data = mem[req.addr];
        if (req.addr == tail_addr)
          req.data |= tail_garbage_or;
      }
      log.push_back({req.addr, req.data, req.byte_en, req.n_bits, req.write});
      bus_item rsp;
      rsp = req;
      rsp.set_id_info(req);
      seq_item_port->item_done();
      seq_item_port->put_response(rsp);
    }
  }
};

class reverse_policy : public uvm_reg_transaction_order_policy {
 public:
  reverse_policy() : uvm_reg_transaction_order_policy("reverse_policy") {}

  virtual void order(std::vector<uvm_reg_bus_op>& q) {
    std::reverse(q.begin(), q.end());
  }
};

class dummy_parent_seq : public uvm_sequence<> {
 public:
  UVM_OBJECT_UTILS(dummy_parent_seq);

  explicit dummy_parent_seq(const std::string& name = "dummy_parent_seq")
      : uvm_sequence<>(name) {}

  virtual uvm_object* create(const std::string& name = "") {
    return new dummy_parent_seq(name);
  }
};

class parent_check_cb : public uvm_reg_cbs {
 public:
  uvm_sequence_base* expected_parent = nullptr;
  int mismatches = 0;
  int calls = 0;

  parent_check_cb() : uvm_reg_cbs("parent_check_cb") {}

  virtual void post_write(uvm_reg_item* rw) {
    ++calls;
    if (rw->parent != expected_parent) ++mismatches;
  }

  virtual void post_read(uvm_reg_item* rw) {
    ++calls;
    if (rw->parent != expected_parent) ++mismatches;
  }
};

class reg32_field12 : public uvm_reg {
 public:
  UVM_OBJECT_UTILS(reg32_field12);

  uvm_reg_field* f = nullptr;

  explicit reg32_field12(const std::string& name = "reg32_field12")
      : uvm_reg(name, 32, UVM_NO_COVERAGE) {}

  virtual void build() {
    f = uvm_reg_field::type_id::create("f");
    f->configure(this, 12, 8, "RW", false, 0x0, true, false, true);
  }
};

class reg20 : public uvm_reg {
 public:
  UVM_OBJECT_UTILS(reg20);

  uvm_reg_field* f = nullptr;

  explicit reg20(const std::string& name = "reg20")
      : uvm_reg(name, 20, UVM_NO_COVERAGE) {}

  virtual void build() {
    f = uvm_reg_field::type_id::create("f");
    f->configure(this, 20, 0, "RW", false, 0x0, true, false, true);
  }
};

class blk_map : public uvm_reg_block {
 public:
  UVM_OBJECT_UTILS(blk_map);

  reg32_field12* r_fld = nullptr;
  reg20* r_20 = nullptr;

  explicit blk_map(const std::string& name = "blk_map")
      : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  virtual void build() {
    default_map = create_map("map", 0, 2, UVM_LITTLE_ENDIAN, true);
    default_map->set_auto_predict(true);

    r_fld = reg32_field12::type_id::create("r_fld");
    r_fld->configure(this);
    r_fld->build();
    default_map->add_reg(r_fld, 0x100, "RW");

    r_20 = reg20::type_id::create("r_20");
    r_20->configure(this);
    r_20->build();
    default_map->add_reg(r_20, 0x200, "RW");

    lock_model();
  }
};

class test : public uvm_test {
 public:
  UVM_COMPONENT_UTILS(test);

  blk_map* blk = nullptr;
  uvm_sequencer<bus_item>* sqr = nullptr;
  bus_driver* drv = nullptr;
  bus_adapter* adp = nullptr;
  int fails = 0;

  explicit test(uvm_component_name name) : uvm_test(name) {}

  virtual void build_phase(uvm_phase& phase) {
    uvm_test::build_phase(phase);
    blk = blk_map::type_id::create("blk");
    blk->build();
    blk->reset();
    sqr = new uvm_sequencer<bus_item>("sqr");
    drv = bus_driver::type_id::create("drv", this);
    adp = bus_adapter::type_id::create("adp");
  }

  virtual void connect_phase(uvm_phase& phase) {
    blk->default_map->set_sequencer(sqr, adp);
    drv->seq_item_port.connect(sqr->seq_item_export);
  }

  virtual void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);

    uvm_status_e status;
    uvm_reg_data_t rval = 0;

    // A. Multi-beat UVM_FIELD byte_en + addrs trimming (16-bit bus, lsb=8, size=12)
    drv->log.clear();
    blk->r_fld->f->write(status, 0xABC);
    bool wr_beats_ok = (drv->log.size() == 2 &&
                        drv->log[0].addr == 0x100 && drv->log[0].byte_en == 0x2 &&
                        drv->log[1].addr == 0x102 && drv->log[1].byte_en == 0x1);
    drv->log.clear();
    blk->r_fld->f->read(status, rval);
    bool rd_beats_ok = (drv->log.size() == 2 &&
                        drv->log[0].addr == 0x100 && drv->log[0].byte_en == 0x2 &&
                        drv->log[1].addr == 0x102 && drv->log[1].byte_en == 0x1 &&
                        rval.to_uint64() == 0xABC);
    std::cout << "field_wr_ok=" << wr_beats_ok << " field_rd_ok=" << rd_beats_ok
              << " rval=0x" << std::hex << rval.to_uint64() << std::dec << std::endl;
    if (!wr_beats_ok || !rd_beats_ok) ++fails;

    // B. Narrow tail beat masking (20-bit reg on 16-bit bus) w/o and w/ policy
    drv->tail_garbage_or = 0xFFF0;
    for (int use_pol = 0; use_pol < 2; ++use_pol) {
      reverse_policy pol;
      blk->default_map->set_transaction_order_policy(use_pol ? &pol : nullptr);

      drv->log.clear();
      uvm_reg_item* item = uvm_reg_item::type_id::create("item");
      item->element = blk->r_20;
      item->element_kind = UVM_REG;
      item->access_kind = UVM_WRITE;
      item->value.resize(1);
      item->value[0] = 0xFFF51234ULL;
      item->map = blk->default_map;
      item->local_map = blk->default_map;
      blk->default_map->do_write(item);
      uvm_reg_item::type_id::destroy(item);

      bool w_tail_ok = false;
      for (const auto& b : drv->log) {
        if (b.addr == 0x202 && b.n_bits == 4 && b.data == 0x5 && (b.byte_en & 0x3) == 0x3)
          w_tail_ok = true;
      }

      blk->r_20->read(status, rval);
      bool r_tail_ok = (rval.to_uint64() == 0x51234ULL);
      std::cout << "pol=" << use_pol << " w_tail_ok=" << w_tail_ok
                << " r_tail_ok=" << r_tail_ok
                << " rval=0x" << std::hex << rval.to_uint64() << std::dec << std::endl;
      if (!w_tail_ok || !r_tail_ok) ++fails;
    }
    blk->default_map->set_transaction_order_policy(nullptr);

    // C. tmp_parent_seq cleanup & rw->parent restoration
    parent_check_cb pcb;
    uvm_reg_cb::add(blk->r_20, &pcb);
    dummy_parent_seq* adp_seq = new dummy_parent_seq("adp_seq");
    adp->parent_sequence = adp_seq;

    pcb.expected_parent = nullptr;
    blk->r_20->write(status, 0x12345);
    blk->r_20->read(status, rval);

    dummy_parent_seq* caller_seq = new dummy_parent_seq("caller_seq");
    pcb.expected_parent = caller_seq;
    blk->r_20->write(status, 0x23456, UVM_FRONTDOOR, nullptr, caller_seq);
    blk->r_20->read(status, rval, UVM_FRONTDOOR, nullptr, caller_seq);
    adp->parent_sequence = nullptr;
    uvm_reg_cb::do_delete(blk->r_20, &pcb);

    std::cout << "parent_cb_calls=" << pcb.calls
              << " parent_mismatches=" << pcb.mismatches << std::endl;
    if (pcb.calls != 4 || pcb.mismatches != 0) ++fails;

    delete caller_seq;
    delete adp_seq;

    std::cout << (fails == 0 ? "** UVM TEST PASSED **" : "!! UVM TEST FAILED !!") << std::endl;
    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  run_test("test");
  return 0;
}
