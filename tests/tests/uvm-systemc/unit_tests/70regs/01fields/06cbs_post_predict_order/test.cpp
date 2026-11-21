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

// Reproducing test for Fix 2 (uvm_reg_cbs::post_predict & callback order):
//   1. uvm_reg_cbs::post_predict value param is uvm_reg_data_t&
//   2. UVM_PREDICT_DIRECT invokes cb->post_predict and returns early on is_busy()
//   3. cb->post_write/cb->post_read execute BEFORE this->post_write/post_read

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

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
    supports_byte_enable = true;
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

class tracked_field : public uvm_reg_field {
 public:
  UVM_OBJECT_UTILS(tracked_field);

  std::vector<std::string> events;

  explicit tracked_field(const std::string& name = "tracked_field")
      : uvm_reg_field(name) {}

  virtual void post_write(uvm_reg_item*) {
    events.push_back("field::post_write");
  }

  virtual void post_read(uvm_reg_item*) {
    events.push_back("field::post_read");
  }
};

class mutating_cb : public uvm_reg_cbs {
 public:
  std::vector<std::string>& events;
  std::vector<uvm_predict_e> predict_kinds;

  mutating_cb(std::vector<std::string>& ev)
      : uvm_reg_cbs("mutating_cb"), events(ev) {}

  virtual void post_write(uvm_reg_item*) {
    events.push_back("cb::post_write");
  }

  virtual void post_read(uvm_reg_item*) {
    events.push_back("cb::post_read");
  }

  virtual void post_predict(uvm_reg_field*,
                            uvm_reg_data_t,
                            uvm_reg_data_t& value,
                            uvm_predict_e kind,
                            uvm_path_e,
                            uvm_reg_map*) {
    predict_kinds.push_back(kind);
    value = (value ^ 0xF) & 0xFF;
  }
};

class busy_driver : public uvm_driver<bus_item> {
 public:
  UVM_COMPONENT_UTILS(busy_driver);

  tracked_field* busy_probe_fld = nullptr;
  bool busy_predict_ret = true;
  uint64_t busy_mirror_after = 0;

  explicit busy_driver(uvm_component_name name) : uvm_driver<bus_item>(name) {}

  virtual void run_phase(uvm_phase&) {
    uint64_t stored = 0;
    while (true) {
      bus_item req;
      seq_item_port->get_next_item(req);
      sc_core::wait(1, sc_core::SC_NS);
      if (busy_probe_fld != nullptr && req.write && req.data == 0xAA) {
        // Parent register is busy right now: UVM_PREDICT_DIRECT must fail and not mutate mirror
        busy_predict_ret = busy_probe_fld->predict(0x77, -1, UVM_PREDICT_DIRECT);
        busy_mirror_after = busy_probe_fld->get_mirrored_value().to_uint64();
      }
      if (req.write)
        stored = req.data;
      else
        req.data = 0x34;
      bus_item rsp;
      rsp = req;
      rsp.set_id_info(req);
      seq_item_port->item_done();
      seq_item_port->put_response(rsp);
    }
  }
};

class reg_one : public uvm_reg {
 public:
  UVM_OBJECT_UTILS(reg_one);

  tracked_field* fld = nullptr;

  explicit reg_one(const std::string& name = "reg_one")
      : uvm_reg(name, 8, UVM_NO_COVERAGE) {}

  virtual void build() {
    fld = tracked_field::type_id::create("fld");
    fld->configure(this, 8, 0, "RW", false, 0x0, true, false, true);
  }
};

class blk_one : public uvm_reg_block {
 public:
  UVM_OBJECT_UTILS(blk_one);

  reg_one* r = nullptr;

  explicit blk_one(const std::string& name = "blk_one")
      : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  virtual void build() {
    default_map = create_map("map", 0, 1, UVM_LITTLE_ENDIAN, true);
    default_map->set_auto_predict(true);
    r = reg_one::type_id::create("r");
    r->configure(this);
    r->build();
    default_map->add_reg(r, 0x0, "RW");
    lock_model();
  }
};

class warn_catcher : public uvm_report_catcher {
 public:
  int caught = 0;

  warn_catcher() : uvm_report_catcher("warn_catcher") {}

  virtual action_e do_catch() {
    if (get_severity() == UVM_WARNING && get_id() == "RegModel") {
      ++caught;
      return CAUGHT;
    }
    return THROW;
  }
};

class test : public uvm_test {
 public:
  UVM_COMPONENT_UTILS(test);

  blk_one* blk = nullptr;
  uvm_sequencer<bus_item>* sqr = nullptr;
  busy_driver* drv = nullptr;
  bus_adapter* adp = nullptr;
  int fails = 0;

  explicit test(uvm_component_name name) : uvm_test(name) {}

  virtual void build_phase(uvm_phase& phase) {
    uvm_test::build_phase(phase);
    blk = blk_one::type_id::create("blk");
    blk->build();
    blk->reset();
    sqr = new uvm_sequencer<bus_item>("sqr");
    drv = busy_driver::type_id::create("drv", this);
    adp = bus_adapter::type_id::create("adp");
  }

  virtual void connect_phase(uvm_phase& phase) {
    blk->default_map->set_sequencer(sqr, adp);
    drv->seq_item_port.connect(sqr->seq_item_export);
  }

  virtual void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);

    mutating_cb cb(blk->r->fld->events);
    uvm_reg_field_cb::add(blk->r->fld, &cb);

    uvm_status_e status;
    uvm_reg_data_t rval = 0;

    // 1. Write: cb::post_write before field::post_write + post_predict mutates value
    blk->r->fld->write(status, 0x12);
    uint64_t m_w = blk->r->fld->get_mirrored_value().to_uint64();
    bool w_ord_ok = (blk->r->fld->events.size() == 2 &&
                     blk->r->fld->events[0] == "cb::post_write" &&
                     blk->r->fld->events[1] == "field::post_write");
    std::cout << "write_order_ok=" << w_ord_ok << " mir_w=0x" << std::hex << m_w << std::dec << std::endl;
    if (!w_ord_ok || m_w != 0x1D) ++fails;

    // 2. Read: cb::post_read before field::post_read + post_predict mutates value
    blk->r->fld->events.clear();
    blk->r->fld->read(status, rval);
    uint64_t m_r = blk->r->fld->get_mirrored_value().to_uint64();
    bool r_ord_ok = (blk->r->fld->events.size() == 2 &&
                     blk->r->fld->events[0] == "cb::post_read" &&
                     blk->r->fld->events[1] == "field::post_read");
    std::cout << "read_order_ok=" << r_ord_ok << " mir_r=0x" << std::hex << m_r << std::dec << std::endl;
    if (!r_ord_ok || m_r != 0x3B) ++fails;

    // 3. Direct predict: invokes post_predict and mutates value
    bool ok_dir = blk->r->fld->predict(0x56, -1, UVM_PREDICT_DIRECT);
    uint64_t m_d = blk->r->fld->get_mirrored_value().to_uint64();
    std::cout << "dir_ok=" << ok_dir << " kinds=" << cb.predict_kinds.size()
              << " mir_d=0x" << std::hex << m_d << std::dec << std::endl;
    if (!ok_dir || cb.predict_kinds.size() != 3 ||
        cb.predict_kinds[2] != UVM_PREDICT_DIRECT || m_d != 0x59) {
      ++fails;
    }

    // 4. Busy UVM_PREDICT_DIRECT must return false and leave mirror untouched
    warn_catcher wc;
    uvm_report_cb::add(nullptr, &wc);
    drv->busy_probe_fld = blk->r->fld;
    blk->r->fld->write(status, 0xAA);
    uvm_report_cb::do_delete(nullptr, &wc);

    std::cout << "busy_ret=" << drv->busy_predict_ret
              << " busy_mir=0x" << std::hex << drv->busy_mirror_after << std::dec
              << " busy_warns=" << wc.caught << std::endl;
    if (drv->busy_predict_ret || drv->busy_mirror_after != 0x59 || wc.caught != 1) {
      ++fails;
    }

    std::cout << (fails == 0 ? "** UVM TEST PASSED **" : "!! UVM TEST FAILED !!") << std::endl;
    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  run_test("test");
  return 0;
}
