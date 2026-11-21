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

// Reproducing test for Fix 5 (uvm_reg_indirect_data & uvm_reg_predictor):
//   1. NO_REG_TESTS resource is set on uvm_reg_indirect_data so built-in
//      sequences skip it cleanly
//   2. Valid and out-of-bounds mirrored index in get_indirect_reg() via
//      uvm_reg_predictor (including > 2^32 truncation test)

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

  uint64_t idx_val = 0;
  uint64_t arr[4] = {0, 0, 0, 0};

  explicit bus_driver(uvm_component_name name) : uvm_driver<bus_item>(name) {}

  virtual void run_phase(uvm_phase&) {
    while (true) {
      bus_item req;
      seq_item_port->get_next_item(req);
      sc_core::wait(1, sc_core::SC_NS);
      if (req.addr == 0x0) {
        if (req.write)
          idx_val = req.data;
        else
          req.data = idx_val;
      } else if (req.addr == 0x8) {
        if (idx_val < 4) {
          if (req.write)
            arr[idx_val] = req.data;
          else
            req.data = arr[idx_val];
        } else if (!req.write) {
          req.data = 0;
        }
      }
      bus_item rsp;
      rsp = req;
      rsp.set_id_info(req);
      seq_item_port->item_done();
      seq_item_port->put_response(rsp);
    }
  }
};

class idx_reg_t : public uvm_reg {
 public:
  UVM_OBJECT_UTILS(idx_reg_t);
  uvm_reg_field* f = nullptr;

  explicit idx_reg_t(const std::string& name = "idx_reg_t")
      : uvm_reg(name, 64, UVM_NO_COVERAGE) {}

  virtual void build() {
    f = uvm_reg_field::type_id::create("f");
    f->configure(this, 64, 0, "RW", false, 0x0, true, false, true);
  }
};

class data_elem_reg_t : public uvm_reg {
 public:
  UVM_OBJECT_UTILS(data_elem_reg_t);
  uvm_reg_field* f = nullptr;

  explicit data_elem_reg_t(const std::string& name = "data_elem_reg_t")
      : uvm_reg(name, 32, UVM_NO_COVERAGE) {}

  virtual void build() {
    f = uvm_reg_field::type_id::create("f");
    f->configure(this, 32, 0, "RW", false, 0x0, true, false, true);
  }
};

class my_indirect_data : public uvm_reg_indirect_data {
 public:
  UVM_OBJECT_UTILS(my_indirect_data);

  explicit my_indirect_data(const std::string& name = "my_indirect_data")
      : uvm_reg_indirect_data(name, 32, UVM_NO_COVERAGE) {}
};

class ind_blk : public uvm_reg_block {
 public:
  UVM_OBJECT_UTILS(ind_blk);

  idx_reg_t* idx = nullptr;
  std::vector<uvm_reg*> tbl;
  my_indirect_data* data_win = nullptr;

  explicit ind_blk(const std::string& name = "ind_blk")
      : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  virtual void build() {
    default_map = create_map("map", 0, 8, UVM_LITTLE_ENDIAN);

    idx = idx_reg_t::type_id::create("idx");
    idx->configure(this);
    idx->build();
    default_map->add_reg(idx, 0x0, "RW");

    for (int i = 0; i < 4; ++i) {
      std::string rname = "tbl_" + std::to_string(i);
      auto* r = data_elem_reg_t::type_id::create(rname);
      r->configure(this);
      r->build();
      tbl.push_back(r);
    }

    data_win = my_indirect_data::type_id::create("data_win");
    data_win->configure(idx, tbl, this);
    default_map->add_reg(data_win, 0x8, "RW");

    lock_model();
  }
};

class err_catcher : public uvm_report_catcher {
 public:
  int caught = 0;

  err_catcher() : uvm_report_catcher("err_catcher") {}

  virtual action_e do_catch() {
    if (get_severity() == UVM_ERROR) {
      ++caught;
      return CAUGHT;
    }
    return THROW;
  }
};

class test : public uvm_test {
 public:
  UVM_COMPONENT_UTILS(test);

  ind_blk* blk = nullptr;
  uvm_reg_block* null_blk = nullptr;
  my_indirect_data* null_idx_win = nullptr;
  my_indirect_data* unconf_win = nullptr;
  data_elem_reg_t* unconf_plain = nullptr;
  uvm_sequencer<bus_item>* sqr = nullptr;
  bus_driver* drv = nullptr;
  bus_adapter* adp = nullptr;
  uvm_reg_predictor<bus_item>* pred = nullptr;
  uvm_reg_predictor<bus_item>* null_pred = nullptr;
  uvm_reg_predictor<bus_item>* standalone_pred = nullptr;
  uvm_reg_map* standalone_map = nullptr;
  data_elem_reg_t* standalone_reg = nullptr;
  idx_reg_t* standalone_reg64 = nullptr;
  uvm_reg_block* bad_idx_blk = nullptr;
  int fails = 0;

  explicit test(uvm_component_name name) : uvm_test(name) {}

  virtual void build_phase(uvm_phase& phase) {
    uvm_test::build_phase(phase);
    blk = ind_blk::type_id::create("blk");
    blk->build();
    blk->reset();

    sqr = new uvm_sequencer<bus_item>("sqr");
    drv = bus_driver::type_id::create("drv", this);
    adp = bus_adapter::type_id::create("adp");
    pred = new uvm_reg_predictor<bus_item>("pred");
    null_pred = new uvm_reg_predictor<bus_item>("null_pred");
    standalone_pred = new uvm_reg_predictor<bus_item>("standalone_pred");
  }

  virtual void connect_phase(uvm_phase& phase) {
    blk->default_map->set_sequencer(sqr, adp);
    drv->seq_item_port.connect(sqr->seq_item_export);
    pred->map = blk->default_map;
    pred->adapter = adp;
  }

  virtual void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);

    // 1. NO_REG_TESTS resource check & built-in hw_reset / bit_bash seqs
    bool has_no_reg_tests =
        (uvm_resource_db<bool>::get_by_name(
             "REG::" + blk->data_win->get_full_name(), "NO_REG_TESTS", false) != nullptr);
    std::cout << "no_reg_tests=" << has_no_reg_tests << std::endl;
    if (!has_no_reg_tests) ++fails;

    for (auto* r : blk->tbl) {
      uvm_resource_db<bool>::set("REG::" + r->get_full_name(),
                                 "NO_REG_TESTS", true, this);
    }

    auto* rst_seq = uvm_reg_hw_reset_seq::type_id::create("rst_seq");
    rst_seq->model = blk;
    rst_seq->start(nullptr);

    auto* bash_seq = uvm_reg_bit_bash_seq::type_id::create("bash_seq");
    bash_seq->model = blk;
    bash_seq->start(nullptr);

    // 2. Predictor valid indirect write
    blk->reset();
    bus_item tx;
    tx.addr = 0x0;
    tx.data = 2;
    tx.write = true;
    pred->bus_in.write(tx);

    tx.addr = 0x8;
    tx.data = 0xCAFEBABEULL;
    tx.write = true;
    pred->bus_in.write(tx);

    bool valid_ok = (blk->tbl[2]->get_mirrored_value().to_uint64() == 0xCAFEBABEULL);
    std::cout << "valid_ind_ok=" << valid_ok << std::endl;
    if (!valid_ok) ++fails;

    // 3. Out-of-bounds index (4) and 2^32 wrap index (0x100000000ULL) via predictor
    err_catcher catcher;
    uvm_report_cb::add(nullptr, &catcher);

    blk->idx->predict(4);
    tx.addr = 0x8;
    tx.data = 0xBEEF;
    tx.write = true;
    pred->bus_in.write(tx);

    blk->idx->predict(0x100000000ULL);
    tx.addr = 0x8;
    tx.data = 0xDEAD;
    tx.write = true;
    pred->bus_in.write(tx);

    uvm_report_cb::do_delete(nullptr, &catcher);

    bool oob_ok = (catcher.caught == 2 &&
                   blk->tbl[0]->get_mirrored_value().to_uint64() == 0);
    std::cout << "oob_errors=" << catcher.caught
              << " tbl0=0x" << std::hex << blk->tbl[0]->get_mirrored_value().to_uint64()
              << std::dec << std::endl;
    if (!oob_ok) ++fails;

    // 4. Null m_idx and unconfigured uvm_reg_indirect_data guards
    {
      // 4a. Unconfigured uvm_reg_indirect_data write/read (default path & frontdoor nullptr)
      err_catcher unconf_catcher;
      uvm_report_cb::add(nullptr, &unconf_catcher);
      unconf_win = my_indirect_data::type_id::create("unconf_win");
      uvm_reg* unconf_reg = unconf_win;
      uvm_status_e st_def_w = UVM_IS_OK, st_def_r = UVM_IS_OK;
      uvm_status_e st_fd_w = UVM_IS_OK, st_fd_r = UVM_IS_OK;
      uvm_reg_data_t rval = 0x1234;
      unconf_reg->write(st_def_w, 0xABCD);
      unconf_reg->read(st_def_r, rval);
      unconf_reg->write(st_fd_w, 0xABCD, UVM_FRONTDOOR, nullptr);
      unconf_reg->read(st_fd_r, rval, UVM_FRONTDOOR, nullptr);
      unconf_plain = data_elem_reg_t::type_id::create("unconf_plain");
      unconf_plain->build();
      uvm_status_e st_pl_w = UVM_IS_OK, st_pl_r = UVM_IS_OK;
      unconf_plain->write(st_pl_w, 0x1111);
      unconf_plain->read(st_pl_r, rval);
      uvm_report_cb::do_delete(nullptr, &unconf_catcher);

      bool unconf_ok = (st_def_w == UVM_NOT_OK && st_def_r == UVM_NOT_OK &&
                        st_fd_w == UVM_NOT_OK && st_fd_r == UVM_NOT_OK &&
                        st_pl_w == UVM_NOT_OK && st_pl_r == UVM_NOT_OK &&
                        unconf_catcher.caught == 8);
      std::cout << "unconf_ok=" << unconf_ok
                << " unconf_errs=" << unconf_catcher.caught << std::endl;
      if (!unconf_ok) ++fails;

      // 4b. configure(nullptr, tbl, blk) mapped at 0x10:
      //     get_local_map / get_address, write/read with default map=nullptr,
      //     and uvm_reg_predictor::write() hitting 0x10
      null_blk = new uvm_reg_block("null_blk", UVM_NO_COVERAGE);
      uvm_reg_map* null_map = null_blk->create_map("map", 0, 4, UVM_LITTLE_ENDIAN);
      null_idx_win = my_indirect_data::type_id::create("null_idx_win");
      null_idx_win->configure(nullptr, std::vector<uvm_reg*>(), null_blk);
      null_map->add_reg(null_idx_win, 0x10, "RW");
      null_blk->lock_model();

      null_pred->map = null_map;
      null_pred->adapter = adp;

      err_catcher null_catcher;
      uvm_report_cb::add(nullptr, &null_catcher);
      uvm_reg* null_reg = null_idx_win;
      uvm_reg_map* null_lmap = null_reg->get_local_map(null_map);
      uvm_reg_addr_t null_addr = null_idx_win->get_address(null_map);

      uvm_status_e st_null_w = UVM_IS_OK, st_null_r = UVM_IS_OK;
      uvm_reg_data_t null_rval = 0;
      null_reg->write(st_null_w, 0x5678);
      null_reg->read(st_null_r, null_rval);

      bus_item null_tx;
      null_tx.addr = 0x10;
      null_tx.data = 0xDEAD;
      null_tx.write = true;
      null_pred->bus_in.write(null_tx);
      uvm_report_cb::do_delete(nullptr, &null_catcher);

      bool null_idx_ok = (null_lmap == nullptr && null_addr == 0 &&
                          st_null_w == UVM_NOT_OK && st_null_r == UVM_NOT_OK &&
                          null_catcher.caught == 8);
      std::cout << "null_idx_ok=" << null_idx_ok
                << " null_errs=" << null_catcher.caught << std::endl;
      if (!null_idx_ok) ++fails;
    }

    // 5. Indirect-element frontdoor write/read, check_on_read, add_frontdoors null m_idx guard,
    //    standalone uvm_reg_map in predictor, and repeated predictor + collision handling
    {
      // 5a. Indirect-element frontdoor write() and read() via uvm_reg_indirect_ftdr_seq
      blk->reset();
      blk->default_map->set_auto_predict(true);

      uvm_status_e st_fw1 = UVM_NOT_OK, st_fw3 = UVM_NOT_OK, st_fr1 = UVM_NOT_OK;
      uvm_reg_data_t frval1 = 0;
      blk->tbl[1]->write(st_fw1, 0x12345678ULL);
      blk->tbl[3]->write(st_fw3, 0xA5A55A5AULL);
      blk->tbl[1]->read(st_fr1, frval1);

      bool ftdr_ok = (st_fw1 == UVM_IS_OK && st_fw3 == UVM_IS_OK && st_fr1 == UVM_IS_OK &&
                      frval1.to_uint64() == 0x12345678ULL &&
                      drv->arr[1] == 0x12345678ULL && drv->arr[3] == 0xA5A55A5AULL &&
                      blk->idx->get_mirrored_value().to_uint64() == 1 &&
                      blk->tbl[1]->get_mirrored_value().to_uint64() == 0x12345678ULL &&
                      blk->tbl[3]->get_mirrored_value().to_uint64() == 0xA5A55A5AULL);
      std::cout << "ftdr_ok=" << ftdr_ok
                << " frval1=0x" << std::hex << frval1.to_uint64() << std::dec << std::endl;
      if (!ftdr_ok) ++fails;

      // 5b. map->set_check_on_read(true) on uvm_reg_indirect_data::read() (match + mismatch)
      blk->default_map->set_check_on_read(true);
      uvm_reg* win_reg = blk->data_win;
      uvm_status_e st_iw = UVM_NOT_OK, st_ir_ok = UVM_NOT_OK, st_ir_mm = UVM_NOT_OK;
      uvm_reg_data_t irval_ok = 0, irval_mm = 0;
      blk->idx->write(st_iw, 2);
      win_reg->write(st_iw, 0xCAFEBABEULL);
      win_reg->read(st_ir_ok, irval_ok);

      err_catcher cor_catcher;
      uvm_report_cb::add(nullptr, &cor_catcher);
      drv->arr[2] = 0xDEADC0DEULL;
      win_reg->read(st_ir_mm, irval_mm);
      uvm_report_cb::do_delete(nullptr, &cor_catcher);

      blk->default_map->set_check_on_read(false);
      blk->default_map->set_auto_predict(false);

      bool cor_ok = (st_ir_ok == UVM_IS_OK && irval_ok.to_uint64() == 0xCAFEBABEULL &&
                     st_ir_mm == UVM_IS_OK && irval_mm.to_uint64() == 0xDEADC0DEULL &&
                     cor_catcher.caught == 1);
      std::cout << "cor_ok=" << cor_ok
                << " cor_errs=" << cor_catcher.caught << std::endl;
      if (!cor_ok) ++fails;

      // 5c. add_frontdoors() with m_idx == nullptr and non-empty m_tbl
      bad_idx_blk = new uvm_reg_block("bad_idx_blk", UVM_NO_COVERAGE);
      uvm_reg_map* bad_map = bad_idx_blk->create_map("map", 0, 4, UVM_LITTLE_ENDIAN);
      auto* bad_elem = data_elem_reg_t::type_id::create("bad_elem");
      bad_elem->configure(bad_idx_blk);
      bad_elem->build();
      auto* bad_win = my_indirect_data::type_id::create("bad_win");
      std::vector<uvm_reg*> bad_tbl = {bad_elem};
      bad_win->configure(nullptr, bad_tbl, bad_idx_blk);
      err_catcher bad_fd_catcher;
      uvm_report_cb::add(nullptr, &bad_fd_catcher);
      bad_map->add_reg(bad_win, 0x0, "RW");
      bad_idx_blk->lock_model();
      uvm_report_cb::do_delete(nullptr, &bad_fd_catcher);
      bool bad_fd_ok = (bad_fd_catcher.caught == 1 && bad_elem->get_frontdoor(bad_map) == nullptr);
      std::cout << "bad_fd_ok=" << bad_fd_ok
                << " bad_fd_errs=" << bad_fd_catcher.caught << std::endl;
      if (!bad_fd_ok) ++fails;

      // 5d. Standalone uvm_reg_map (m_parent == nullptr) in uvm_reg_predictor + collision + repeat
      standalone_map = new uvm_reg_map("standalone_map");
      standalone_map->configure(nullptr, 0x100, 4, UVM_LITTLE_ENDIAN, true);
      standalone_reg = data_elem_reg_t::type_id::create("standalone_reg");
      standalone_reg->configure(nullptr);
      standalone_reg->build();
      standalone_reg64 = idx_reg_t::type_id::create("standalone_reg64");
      standalone_reg64->configure(nullptr);
      standalone_reg64->build();
      standalone_map->add_reg(standalone_reg, 0x0, "RW");
      standalone_map->add_reg(standalone_reg64, 0x4, "RW");
      standalone_map->m_init_address_map();

      standalone_pred->map = standalone_map;
      standalone_pred->adapter = adp;

      bus_item stx;
      stx.addr = 0x100;
      stx.data = 0x55AA1234ULL;
      stx.write = true;
      standalone_pred->bus_in.write(stx);

      // Multi-beat collision on standalone_reg64 (0x104, 0x108), then valid 2-beat completion
      err_catcher col_catcher;
      uvm_report_cb::add(nullptr, &col_catcher);
      stx.addr = 0x104;
      stx.data = 0x11112222ULL;
      stx.write = true;
      standalone_pred->bus_in.write(stx);
      standalone_pred->bus_in.write(stx);  // collision
      uvm_report_cb::do_delete(nullptr, &col_catcher);

      stx.addr = 0x104;
      stx.data = 0xCAFE0001ULL;
      standalone_pred->bus_in.write(stx);
      stx.addr = 0x108;
      stx.data = 0xBEEF0002ULL;
      standalone_pred->bus_in.write(stx);

      // Repeated predictor transactions (write/read)
      for (int iter = 0; iter < 50; ++iter) {
        bus_item rtx;
        rtx.addr = 0x0;
        rtx.data = static_cast<uint64_t>(iter % 4);
        rtx.write = true;
        pred->bus_in.write(rtx);

        rtx.addr = 0x8;
        rtx.data = 0x10000ULL + static_cast<uint64_t>(iter);
        rtx.write = true;
        pred->bus_in.write(rtx);
      }

      bool standalone_ok =
          (standalone_reg->get_mirrored_value().to_uint64() == 0x55AA1234ULL &&
           col_catcher.caught == 1 &&
           standalone_reg64->get_mirrored_value().to_uint64() == 0xBEEF0002CAFE0001ULL &&
           blk->tbl[1]->get_mirrored_value().to_uint64() == (0x10000ULL + 49));
      std::cout << "standalone_ok=" << standalone_ok
                << " col_errs=" << col_catcher.caught
                << " reg64=0x" << std::hex << standalone_reg64->get_mirrored_value().to_uint64()
                << std::dec << std::endl;
      if (!standalone_ok) ++fails;
    }

    std::cout << (fails == 0 ? "** UVM TEST PASSED **" : "!! UVM TEST FAILED !!") << std::endl;
    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  run_test("test");
  return 0;
}
