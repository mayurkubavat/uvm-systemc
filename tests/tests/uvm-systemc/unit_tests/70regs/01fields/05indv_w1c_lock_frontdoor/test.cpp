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

// Reproducing test for Fix 1 (uvm_reg_field):
//   1. Sibling field write value_adjust (else-if/else chain + W1CRS typo)
//      and set()/m_update() else-if/else chain
//   2. Early return from do_write()/do_read() unlocks m_atomic & clears
//      m_read_in_progress
//   3. Virtual is_indv_accessible(), frontdoor dispatch in do_write()/do_read(),
//      and adapter == nullptr null guard

#include <cstdint>
#include <iostream>
#include <map>
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
    supports_byte_enable = false;
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

class bus_driver : public uvm_driver<bus_item> {
 public:
  UVM_COMPONENT_UTILS(bus_driver);

  std::vector<uint64_t> writes;
  std::map<uint64_t, uint64_t> mem;

  explicit bus_driver(uvm_component_name name) : uvm_driver<bus_item>(name) {}

  virtual void run_phase(uvm_phase&) {
    while (true) {
      bus_item req;
      seq_item_port->get_next_item(req);
      sc_core::wait(1, sc_core::SC_NS);
      if (req.write) {
        writes.push_back(req.data);
        mem[req.addr] = req.data;
      } else {
        req.data = mem[req.addr];
      }
      bus_item rsp;
      rsp = req;
      rsp.set_id_info(req);
      seq_item_port->item_done();
      seq_item_port->put_response(rsp);
    }
  }
};

class indv_field : public uvm_reg_field {
 public:
  UVM_OBJECT_UTILS(indv_field);

  bool test_post_write_predict = false;
  bool post_write_predict_ret = false;

  explicit indv_field(const std::string& name = "indv_field")
      : uvm_reg_field(name) {}

  virtual bool is_indv_accessible(uvm_path_e, uvm_reg_map*) const {
    return true;
  }

  virtual void post_write(uvm_reg_item*) {
    if (test_post_write_predict) {
      post_write_predict_ret = predict(0x55, -1, UVM_PREDICT_DIRECT);
    }
  }
};

class field_backdoor : public uvm_reg_backdoor {
 public:
  UVM_OBJECT_UTILS(field_backdoor);

  int write_calls = 0;
  int read_calls = 0;
  uvm_reg_data_t stored = 0;

  explicit field_backdoor(const std::string& name = "field_backdoor")
      : uvm_reg_backdoor(name) {}

  virtual void write(uvm_reg_item* rw) {
    ++write_calls;
    stored = rw->value[0];
    rw->status = UVM_IS_OK;
  }

  virtual void read(uvm_reg_item* rw) {
    ++read_calls;
    rw->value[0] = stored;
    rw->status = UVM_IS_OK;
  }
};

class field_frontdoor : public uvm_reg_frontdoor {
 public:
  UVM_OBJECT_UTILS(field_frontdoor);

  int write_calls = 0;
  int read_calls = 0;
  uvm_elem_kind_e last_kind = UVM_REG;
  uvm_reg_data_t stored = 0;

  explicit field_frontdoor(const std::string& name = "field_frontdoor")
      : uvm_reg_frontdoor(name) {}

  virtual void body() {
    last_kind = rw_info->element_kind;
    if (rw_info->access_kind == UVM_WRITE) {
      ++write_calls;
      stored = rw_info->value[0];
    } else {
      ++read_calls;
      rw_info->value[0] = stored;
    }
    rw_info->status = UVM_IS_OK;
  }
};

class mix_reg : public uvm_reg {
 public:
  UVM_OBJECT_UTILS(mix_reg);

  uvm_reg_field* f_rw = nullptr;
  uvm_reg_field* f_w1c = nullptr;
  uvm_reg_field* f_w1crs = nullptr;
  uvm_reg_field* f_w0c = nullptr;
  uvm_reg_field* f_ro = nullptr;

  explicit mix_reg(const std::string& name = "mix_reg")
      : uvm_reg(name, 32, UVM_NO_COVERAGE) {}

  virtual void build() {
    f_rw = uvm_reg_field::type_id::create("f_rw");
    f_rw->configure(this, 8, 0, "RW", false, 0x0, true, false, true);

    f_w1c = uvm_reg_field::type_id::create("f_w1c");
    f_w1c->configure(this, 1, 8, "W1C", false, 0x1, true, false, true);

    f_w1crs = uvm_reg_field::type_id::create("f_w1crs");
    f_w1crs->configure(this, 1, 9, "W1CRS", false, 0x1, true, false, true);

    f_w0c = uvm_reg_field::type_id::create("f_w0c");
    f_w0c->configure(this, 1, 10, "W0C", false, 0x1, true, false, true);

    f_ro = uvm_reg_field::type_id::create("f_ro");
    f_ro->configure(this, 1, 11, "RO", false, 0x1, true, false, true);
  }
};

class fd_reg : public uvm_reg {
 public:
  UVM_OBJECT_UTILS(fd_reg);

  indv_field* f_indv = nullptr;

  explicit fd_reg(const std::string& name = "fd_reg")
      : uvm_reg(name, 32, UVM_NO_COVERAGE) {}

  virtual void build() {
    f_indv = indv_field::type_id::create("f_indv");
    f_indv->configure(this, 8, 0, "RW", false, 0x0, true, false, true);
  }
};

class reg_block : public uvm_reg_block {
 public:
  UVM_OBJECT_UTILS(reg_block);

  mix_reg* r_mix = nullptr;
  fd_reg* r_fd = nullptr;
  uvm_reg_map* map1 = nullptr;
  uvm_reg_map* map2 = nullptr;

  explicit reg_block(const std::string& name = "reg_block")
      : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  virtual void build() {
    map1 = create_map("map1", 0x0, 4, UVM_LITTLE_ENDIAN, false);
    map1->set_auto_predict(true);
    map2 = create_map("map2", 0x100, 4, UVM_LITTLE_ENDIAN, false);
    map2->set_auto_predict(true);

    r_mix = mix_reg::type_id::create("r_mix");
    r_mix->configure(this);
    r_mix->build();
    map1->add_reg(r_mix, 0x0, "RW");
    // r_mix is intentionally NOT added to map2

    r_fd = fd_reg::type_id::create("r_fd");
    r_fd->configure(this);
    r_fd->build();
    map1->add_reg(r_fd, 0x10, "RW");

    default_map = map1;
    lock_model();
  }
};

class err_catcher : public uvm_report_catcher {
 public:
  int caught = 0;

  err_catcher() : uvm_report_catcher("err_catcher") {}

  virtual action_e do_catch() {
    if (get_severity() == UVM_ERROR &&
        (get_id() == "RegModel" || get_id() == "uvm_reg_field" ||
         get_id() == "mix_reg")) {
      ++caught;
      return CAUGHT;
    }
    return THROW;
  }
};

class test : public uvm_test {
 public:
  UVM_COMPONENT_UTILS(test);

  reg_block* blk = nullptr;
  uvm_sequencer<bus_item>* sqr1 = nullptr;
  uvm_sequencer<bus_item>* sqr2 = nullptr;
  bus_driver* drv1 = nullptr;
  bus_adapter* adp1 = nullptr;
  field_frontdoor* fd = nullptr;
  field_backdoor* bkdr = nullptr;
  mix_reg* parentless_reg = nullptr;
  uvm_reg_map* standalone_map = nullptr;
  uvm_reg_field* unconf_f = nullptr;
  int fails = 0;

  explicit test(uvm_component_name name) : uvm_test(name) {}

  virtual void build_phase(uvm_phase& phase) {
    uvm_test::build_phase(phase);
    blk = reg_block::type_id::create("blk");
    blk->build();
    blk->reset();

    sqr1 = new uvm_sequencer<bus_item>("sqr1");
    sqr2 = new uvm_sequencer<bus_item>("sqr2");
    drv1 = bus_driver::type_id::create("drv1", this);
    adp1 = bus_adapter::type_id::create("adp1");
    fd = field_frontdoor::type_id::create("fd");
    bkdr = field_backdoor::type_id::create("bkdr");
  }

  virtual void connect_phase(uvm_phase& phase) {
    blk->map1->set_sequencer(sqr1, adp1);
    blk->map2->set_sequencer(sqr2, nullptr);
    drv1->seq_item_port.connect(sqr1->seq_item_export);
    blk->r_fd->set_frontdoor(fd, blk->map1);
    blk->r_fd->set_backdoor(bkdr);
  }

  virtual void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);

    uvm_status_e status;
    uvm_reg_data_t rval = 0;

    // 1. Sibling write value_adjust & set()/m_update()
    blk->r_mix->predict(0x0F00);
    blk->r_mix->f_rw->write(status, 0x55, UVM_FRONTDOOR, blk->map1);
    uint64_t bus_w0 = drv1->writes.empty() ? 0xDEAD : drv1->writes.back();
    std::cout << "sibling_write=0x" << std::hex << bus_w0 << std::dec << std::endl;
    if (bus_w0 != 0x0455) ++fails;

    blk->r_mix->f_ro->set(0x0);
    std::cout << "ro_after_set0=" << blk->r_mix->f_ro->get().to_uint64() << std::endl;
    if (blk->r_mix->f_ro->get().to_uint64() != 1) ++fails;

    blk->r_mix->f_w1c->predict(1);
    blk->r_mix->f_w1c->set(1);
    blk->r_mix->update(status, UVM_FRONTDOOR, blk->map1);
    uint64_t bus_w1 = drv1->writes.size() < 2 ? 0xDEAD : drv1->writes.back();
    std::cout << "w1c_update=0x" << std::hex << ((bus_w1 >> 8) & 0x1) << std::dec << std::endl;
    if (((bus_w1 >> 8) & 0x1) != 1) ++fails;

    // 2. Early-return unlock & m_read_in_progress clear
    // Spawn thread1b (distinct sc_process_handle) for unmapped write & read so
    // m_atomic_check_lock(true) actually acquires m_atomic.
    err_catcher catcher;
    uvm_report_cb::add(nullptr, &catcher);

    bool thread1b_done = false;
    sc_core::sc_spawn([&]() {
      uvm_status_e st1 = UVM_IS_OK;
      uvm_reg_data_t rv1 = 0;
      blk->r_mix->f_rw->write(st1, 0x11, UVM_FRONTDOOR, blk->map2);
      blk->r_mix->f_rw->read(st1, rv1, UVM_FRONTDOOR, blk->map2);
      thread1b_done = true;
    });
    sc_core::wait(5, sc_core::SC_NS);
    uvm_report_cb::do_delete(nullptr, &catcher);

    bool stuck_reading =
        (blk->r_mix->f_rw->convert2string().find("currently being read") != std::string::npos);
    std::cout << "caught_unmapped=" << catcher.caught
              << " thread1b=" << thread1b_done
              << " stuck_reading=" << stuck_reading << std::endl;
    if (catcher.caught != 2 || !thread1b_done || stuck_reading) ++fails;

    // 3. Virtual is_indv_accessible + frontdoor dispatch on field + adapter==nullptr
    blk->r_fd->f_indv->write(status, 0xAB, UVM_FRONTDOOR, blk->map1);
    uvm_elem_kind_e wk = fd->last_kind;
    blk->r_fd->f_indv->read(status, rval, UVM_FRONTDOOR, blk->map1);
    uvm_elem_kind_e rk = fd->last_kind;
    std::cout << "fd_w=" << fd->write_calls << " wk=" << (wk == UVM_FIELD)
              << " fd_r=" << fd->read_calls << " rk=" << (rk == UVM_FIELD)
              << " rval=0x" << std::hex << rval.to_uint64() << std::dec << std::endl;
    if (fd->write_calls != 1 || wk != UVM_FIELD ||
        fd->read_calls != 1 || rk != UVM_FIELD || rval.to_uint64() != 0xAB) {
      ++fails;
    }

    // 4. Verify thread2 does NOT deadlock on m_atomic after thread1b early return
    bool thread2_done = false;
    sc_core::sc_process_handle h = sc_core::sc_spawn([&]() {
      uvm_status_e st2 = UVM_NOT_OK;
      blk->r_mix->f_rw->write(st2, 0x22, UVM_FRONTDOOR, blk->map1);
      if (st2 == UVM_IS_OK) thread2_done = true;
    });
    sc_core::wait(20, sc_core::SC_NS);
    if (!thread2_done && h.valid()) h.kill();
    std::cout << "thread2_done=" << thread2_done << std::endl;
    if (!thread2_done) ++fails;

    bool no_adp_indv = blk->r_mix->f_rw->is_indv_accessible(UVM_FRONTDOOR, blk->map2);
    std::cout << "no_adp_indv=" << no_adp_indv << std::endl;

    // 5. Overridden is_indv_accessible + predict(UVM_PREDICT_DIRECT) inside post_write
    blk->r_fd->f_indv->test_post_write_predict = true;
    blk->r_fd->f_indv->write(status, 0x33, UVM_FRONTDOOR, blk->map1);
    blk->r_fd->f_indv->test_post_write_predict = false;
    bool pw_pred_ok = (blk->r_fd->f_indv->post_write_predict_ret &&
                       blk->r_fd->f_indv->get_mirrored_value().to_uint64() == 0x55);
    std::cout << "indv_pw_pred_ok=" << pw_pred_ok << std::endl;
    if (!pw_pred_ok) ++fails;

    // 6. Overridden is_indv_accessible + UVM_BACKDOOR write & read (must not crash)
    uvm_status_e bk_w_st = UVM_NOT_OK;
    uvm_status_e bk_r_st = UVM_NOT_OK;
    uvm_reg_data_t bk_rval = 0;
    blk->r_fd->f_indv->write(bk_w_st, 0x7E, UVM_BACKDOOR);
    blk->r_fd->f_indv->read(bk_r_st, bk_rval, UVM_BACKDOOR);
    bool bk_ok = (bk_w_st == UVM_IS_OK && bk_r_st == UVM_IS_OK &&
                  bkdr->write_calls == 1 && bkdr->read_calls == 2 &&
                  bk_rval.to_uint64() == 0x7E &&
                  blk->r_fd->f_indv->get_mirrored_value().to_uint64() == 0x7E);
    std::cout << "indv_bkdr_ok=" << bk_ok
              << " rval=0x" << std::hex << bk_rval.to_uint64() << std::dec << std::endl;
    if (!bk_ok) ++fails;

    // 7. Null-parent uvm_reg, standalone uvm_reg_map, and unconfigured uvm_reg_field safety
    {
      err_catcher null_catcher;
      uvm_report_cb::add(nullptr, &null_catcher);

      parentless_reg = mix_reg::type_id::create("parentless_reg");
      parentless_reg->build();

      // 7a. Parentless uvm_reg UVM_BACKDOOR write/read + peek/poke/mirror + HDL path queries
      uvm_status_e st_bd_w = UVM_IS_OK, st_bd_r = UVM_IS_OK;
      uvm_status_e st_pk = UVM_IS_OK, st_po = UVM_IS_OK, st_mir = UVM_IS_OK;
      uvm_reg_data_t tmp_val = 0;
      std::vector<uvm_hdl_path_concat> hdl_paths;
      bool has_hdl = parentless_reg->has_hdl_path();
      parentless_reg->get_hdl_path(hdl_paths);
      parentless_reg->get_full_hdl_path(hdl_paths);
      parentless_reg->write(st_bd_w, 0x12, UVM_BACKDOOR, nullptr);
      parentless_reg->read(st_bd_r, tmp_val, UVM_BACKDOOR, nullptr);
      parentless_reg->poke(st_po, 0x34);
      parentless_reg->peek(st_pk, tmp_val);
      parentless_reg->mirror(st_mir);

      // 7b. uvm_reg_field on parentless uvm_reg (UVM_DEFAULT_PATH write/read)
      uvm_status_e st_pf_w = UVM_IS_OK, st_pf_r = UVM_IS_OK;
      parentless_reg->f_rw->write(st_pf_w, 0x33);
      parentless_reg->f_rw->read(st_pf_r, tmp_val);

      // 7c. Unconfigured uvm_reg_field (m_parent == nullptr)
      unconf_f = uvm_reg_field::type_id::create("unconf_f");
      std::string unconf_name = unconf_f->get_full_name();
      uvm_status_e st_uf_w = UVM_IS_OK, st_uf_r = UVM_IS_OK;
      uvm_status_e st_uf_pk = UVM_IS_OK, st_uf_po = UVM_IS_OK, st_uf_mir = UVM_IS_OK;
      unconf_f->write(st_uf_w, 0x44);
      unconf_f->read(st_uf_r, tmp_val);
      unconf_f->poke(st_uf_po, 0x44);
      unconf_f->peek(st_uf_pk, tmp_val);
      unconf_f->mirror(st_uf_mir);

      uvm_report_cb::do_delete(nullptr, &null_catcher);

      // 7d. Parentless uvm_reg added to standalone uvm_reg_map with auto_predict(true)
      standalone_map = new uvm_reg_map("standalone_map");
      standalone_map->configure(nullptr, 0x200, 4, UVM_LITTLE_ENDIAN, false);
      standalone_map->set_sequencer(sqr1, adp1);
      standalone_map->set_auto_predict(true);
      standalone_map->add_reg(parentless_reg, 0x0, "RW");
      standalone_map->m_init_address_map();

      uvm_status_e st_sm_w = UVM_NOT_OK, st_sm_r = UVM_NOT_OK;
      uvm_reg_data_t sm_rval = 0;
      parentless_reg->write(st_sm_w, 0xA5, UVM_FRONTDOOR, standalone_map);
      parentless_reg->read(st_sm_r, sm_rval, UVM_FRONTDOOR, standalone_map);

      bool parentless_ok =
          (!has_hdl && hdl_paths.empty() &&
           st_bd_w == UVM_NOT_OK && st_bd_r == UVM_NOT_OK &&
           st_pk == UVM_NOT_OK && st_po == UVM_NOT_OK && st_mir == UVM_NOT_OK &&
           st_pf_w == UVM_NOT_OK && st_pf_r == UVM_NOT_OK &&
           unconf_name == "unconf_f" &&
           st_uf_w == UVM_NOT_OK && st_uf_r == UVM_NOT_OK &&
           st_uf_pk == UVM_NOT_OK && st_uf_po == UVM_NOT_OK && st_uf_mir == UVM_NOT_OK &&
           st_sm_w == UVM_IS_OK && st_sm_r == UVM_IS_OK &&
           (sm_rval.to_uint64() & 0xff) == 0xA5 &&
           (parentless_reg->get_mirrored_value().to_uint64() & 0xff) == 0xA5 &&
           null_catcher.caught == 12);
      std::cout << "parentless_ok=" << parentless_ok
                << " null_errs=" << null_catcher.caught
                << " sm_rval=0x" << std::hex << (sm_rval.to_uint64() & 0xff) << std::dec
                << std::endl;
      if (!parentless_ok) ++fails;
    }

    std::cout << (fails == 0 ? "** UVM TEST PASSED **" : "!! UVM TEST FAILED !!") << std::endl;
    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  run_test("test");
  return 0;
}
