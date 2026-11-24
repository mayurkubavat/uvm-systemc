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

// Unit & integration test for uvm_reg_tlm_adapter:
// - Direct reg2bus/bus2reg translation (read/write/burst, byte_en, n_bits==0,
//   UVM_FIELD shifted lane expansion, error responses, empty byte_en -> -1,
//   null item fatal, wrong type error)
// - End-to-end RAL frontdoor read/write/mirror + field access + 64b multi-beat
//   + uvm_mem + uvm_reg_predictor<uvm_tlm_gp> over uvm_tlm_b_initiator/target_socket

#include <systemc>
#include <uvm>

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace uvm;

class reg_tlm_msg_catcher : public uvm_report_catcher
{
 public:
  int null_item_fatal = 0;
  int wrong_type_error = 0;

  virtual action_e do_catch()
  {
    const std::string id = get_id();
    if (get_severity() == UVM_FATAL && id == "REG/NULL_ITEM")
    {
      ++null_item_fatal;
      set_severity(UVM_INFO);
      return CAUGHT;
    }
    if (get_severity() == UVM_ERROR && id == "WRONG_TYPE")
    {
      ++wrong_type_error;
      return CAUGHT;
    }
    return THROW;
  }
};

class dummy_seq_item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(dummy_seq_item);
  explicit dummy_seq_item(const std::string& name = "dummy_seq_item")
    : uvm_sequence_item(name) {}
};

class tlm_bus_target : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(tlm_bus_target);

  uvm_tlm_b_target_socket<uvm_tlm_gp, tlm_bus_target> target_skt;
  std::map<uint64_t, unsigned char> mem;

  explicit tlm_bus_target(uvm_component_name name)
    : uvm_component(name),
      target_skt("target_skt", this) {}

  void b_transport(uvm_tlm_gp& gp, uvm_tlm_time& delay)
  {
    delay.incr(1.0, 1.0e-9);
    const uint64_t base = gp.get_address();
    const unsigned int len = gp.get_data_length();
    const unsigned int be_len = gp.get_byte_enable_length();

    if (gp.is_write())
    {
      for (unsigned int i = 0; i < len && i < gp.m_data.size(); ++i)
      {
        bool en = (be_len == 0 || gp.m_byte_enable.empty()) ||
                  (gp.m_byte_enable[i % be_len] != UVM_TLM_BYTE_DISABLED);
        if (en)
          mem[base + i] = gp.m_data[i];
      }
      gp.set_response_status(UVM_TLM_OK_RESPONSE);
    }
    else if (gp.is_read())
    {
      gp.m_data.resize(len);
      for (unsigned int i = 0; i < len; ++i)
      {
        auto it = mem.find(base + i);
        gp.m_data[i] = (it != mem.end()) ? it->second : 0;
      }
      gp.set_response_status(UVM_TLM_OK_RESPONSE);
    }
  }
};

class tlm_bus_driver : public uvm_driver<uvm_tlm_gp>
{
 public:
  UVM_COMPONENT_UTILS(tlm_bus_driver);

  uvm_tlm_b_initiator_socket<uvm_tlm_gp> init_skt;
  uvm_analysis_port<uvm_tlm_gp> ap;
  std::vector<uvm_tlm_gp> observed;

  explicit tlm_bus_driver(uvm_component_name name)
    : uvm_driver<uvm_tlm_gp>(name),
      init_skt("init_skt"),
      ap("ap") {}

  virtual void run_phase(uvm_phase&)
  {
    while (true)
    {
      uvm_tlm_gp req;
      seq_item_port->get_next_item(req);
      sc_core::wait(1, sc_core::SC_NS);

      uvm_tlm_time delay("delay");
      init_skt.b_transport(req, delay);
      observed.push_back(req);
      ap.write(req);

      uvm_tlm_gp rsp = req;
      rsp.set_id_info(req);
      seq_item_port->item_done(rsp);
    }
  }
};

class reg32_fields : public uvm_reg
{
 public:
  UVM_OBJECT_UTILS(reg32_fields);

  uvm_reg_field* f_lo = nullptr;
  uvm_reg_field* f_rsv = nullptr;
  uvm_reg_field* f_mid = nullptr;
  uvm_reg_field* f_hi = nullptr;

  explicit reg32_fields(const std::string& name = "reg32_fields")
    : uvm_reg(name, 32, UVM_NO_COVERAGE) {}

  virtual void build()
  {
    f_lo = uvm_reg_field::type_id::create("f_lo");
    f_lo->configure(this, 8, 0, "RW", false, 0x0, true, false, true);
    f_rsv = uvm_reg_field::type_id::create("f_rsv");
    f_rsv->configure(this, 8, 8, "RW", false, 0x0, true, false, true);
    f_mid = uvm_reg_field::type_id::create("f_mid");
    f_mid->configure(this, 8, 16, "RW", false, 0x0, true, false, true);
    f_hi = uvm_reg_field::type_id::create("f_hi");
    f_hi->configure(this, 8, 24, "RW", false, 0x0, true, false, true);
  }
};

class reg64_wide : public uvm_reg
{
 public:
  UVM_OBJECT_UTILS(reg64_wide);

  uvm_reg_field* val = nullptr;

  explicit reg64_wide(const std::string& name = "reg64_wide")
    : uvm_reg(name, 64, UVM_NO_COVERAGE) {}

  virtual void build()
  {
    val = uvm_reg_field::type_id::create("val");
    val->configure(this, 64, 0, "RW", false, 0x0, true, false, true);
  }
};

class tlm_reg_blk : public uvm_reg_block
{
 public:
  UVM_OBJECT_UTILS(tlm_reg_blk);

  reg32_fields* r32 = nullptr;
  reg64_wide* r64 = nullptr;
  uvm_mem* mem = nullptr;

  explicit tlm_reg_blk(const std::string& name = "tlm_reg_blk")
    : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  virtual void build()
  {
    default_map = create_map("map", 0x0, 4, UVM_LITTLE_ENDIAN, true);
    default_map->set_auto_predict(false);

    r32 = reg32_fields::type_id::create("r32");
    r32->configure(this);
    r32->build();
    default_map->add_reg(r32, 0x100, "RW");

    r64 = reg64_wide::type_id::create("r64");
    r64->configure(this);
    r64->build();
    default_map->add_reg(r64, 0x110, "RW");

    mem = new uvm_mem("mem", 16, 32, "RW", UVM_NO_COVERAGE);
    mem->configure(this);
    default_map->add_mem(mem, 0x200, "RW");

    lock_model();
  }
};

class tlm_adapter_test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(tlm_adapter_test);

  tlm_reg_blk* blk = nullptr;
  uvm_sequencer<uvm_tlm_gp>* sqr = nullptr;
  tlm_bus_driver* drv = nullptr;
  tlm_bus_target* tgt = nullptr;
  uvm_reg_tlm_adapter* adapter = nullptr;
  uvm_reg_predictor<uvm_tlm_gp>* predictor = nullptr;
  bool all_ok = true;

  explicit tlm_adapter_test(uvm_component_name name)
    : uvm_test(name) {}

  void check(bool cond, const std::string& msg)
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("TLM_ADAPTER_TEST", "FAILED: " + msg);
    }
  }

  virtual void build_phase(uvm_phase& phase)
  {
    uvm_test::build_phase(phase);
    blk = tlm_reg_blk::type_id::create("blk");
    blk->build();
    blk->reset();

    sqr = new uvm_sequencer<uvm_tlm_gp>("sqr");
    drv = tlm_bus_driver::type_id::create("drv", this);
    tgt = tlm_bus_target::type_id::create("tgt", this);
    adapter = uvm_reg_tlm_adapter::type_id::create("adapter");
    predictor = uvm_reg_predictor<uvm_tlm_gp>::type_id::create("predictor", this);
  }

  virtual void connect_phase(uvm_phase& phase)
  {
    uvm_test::connect_phase(phase);
    blk->default_map->set_sequencer(sqr, adapter);
    drv->seq_item_port.connect(sqr->seq_item_export);
    drv->init_skt.connect(tgt->target_skt);

    predictor->map = blk->default_map;
    predictor->adapter = adapter;
    drv->ap.connect(predictor->bus_in);
  }

  virtual void run_phase(uvm_phase& phase)
  {
    phase.raise_objection(this);

    // 1. Direct unit checks on uvm_reg_tlm_adapter
    {
      check(adapter->supports_byte_enable == true, "supports_byte_enable == true");
      check(adapter->provides_responses == true, "provides_responses == true");

      uvm_reg_bus_op rw;
      rw.kind = UVM_WRITE;
      rw.addr = 0x104;
      rw.data = 0xA1B2C3D4ULL;
      rw.n_bits = 32;
      rw.byte_en = 0x0D; // bits 0, 2, 3

      uvm_sequence_item* item = adapter->reg2bus(rw);
      uvm_tlm_gp* gp = dynamic_cast<uvm_tlm_gp*>(item);
      check(gp != nullptr, "reg2bus returns non-null uvm_tlm_gp");
      if (gp != nullptr)
      {
        check(gp->is_write() && gp->get_address() == 0x104ULL &&
              gp->get_data_length() == 4 && gp->get_streaming_width() == 4 &&
              gp->get_byte_enable_length() == 4 &&
              gp->get_response_status() == UVM_TLM_INCOMPLETE_RESPONSE,
              "reg2bus header fields");
        check(gp->m_data == std::vector<unsigned char>({0xD4, 0xC3, 0xB2, 0xA1}),
              "reg2bus little-endian data");
        check(gp->m_byte_enable == std::vector<unsigned char>({
                UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED,
                UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_ENABLED}),
              "reg2bus byte enables");

        gp->set_response_status(UVM_TLM_OK_RESPONSE);
        uvm_reg_bus_op rw_out;
        adapter->bus2reg(gp, rw_out);
        check(rw_out.kind == UVM_WRITE && rw_out.addr == 0x104ULL &&
              rw_out.data == 0xA1B2C3D4ULL && rw_out.n_bits == 32 &&
              rw_out.byte_en.to_uint() == 0x0Du && rw_out.status == UVM_IS_OK,
              "bus2reg write round-trip");

        gp->set_response_status(UVM_TLM_GENERIC_ERROR_RESPONSE);
        adapter->bus2reg(gp, rw_out);
        check(rw_out.status == UVM_NOT_OK, "bus2reg error status -> UVM_NOT_OK");

        // Modulo byte-enable wrapping (data_len=4, be_len=2)
        gp->set_response_status(UVM_TLM_OK_RESPONSE);
        gp->set_byte_enable_length(2);
        gp->set_byte_enable({UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED});
        adapter->bus2reg(gp, rw_out);
        check(rw_out.byte_en.to_uint() == 0x5u,
              "bus2reg wraps modulo byte_enable_length (0x5)");

        gp->set_byte_enable_length(0);
        gp->m_byte_enable.clear();
        adapter->bus2reg(gp, rw_out);
        check(rw_out.byte_en.and_reduce(),
              "bus2reg empty byte_enable -> all-ones (-1)");
      }

      // UVM_BURST_WRITE & UVM_BURST_READ mapping
      {
        uvm_reg_bus_op rw_bw;
        rw_bw.kind = UVM_BURST_WRITE;
        rw_bw.addr = 0x300;
        rw_bw.data = 0x1234;
        rw_bw.n_bits = 16;
        rw_bw.byte_en = 0x3;
        uvm_tlm_gp* gp_bw = dynamic_cast<uvm_tlm_gp*>(adapter->reg2bus(rw_bw));
        check(gp_bw != nullptr && gp_bw->is_write() && gp_bw->get_data_length() == 2,
              "reg2bus UVM_BURST_WRITE -> UVM_TLM_WRITE_COMMAND");

        uvm_reg_bus_op rw_br;
        rw_br.kind = UVM_BURST_READ;
        rw_br.addr = 0x300;
        rw_br.data = 0;
        rw_br.n_bits = 16;
        rw_br.byte_en = 0x3;
        uvm_tlm_gp* gp_br = dynamic_cast<uvm_tlm_gp*>(adapter->reg2bus(rw_br));
        check(gp_br != nullptr && gp_br->is_read() && gp_br->get_data_length() == 2,
              "reg2bus UVM_BURST_READ -> UVM_TLM_READ_COMMAND");
      }

      // n_bits == 0 guard
      uvm_reg_bus_op rw_zero;
      rw_zero.kind = UVM_READ;
      rw_zero.addr = 0x20;
      rw_zero.data = 0;
      rw_zero.n_bits = 0;
      rw_zero.byte_en = 0;
      uvm_tlm_gp* gp_zero = dynamic_cast<uvm_tlm_gp*>(adapter->reg2bus(rw_zero));
      check(gp_zero != nullptr && gp_zero->is_read() && gp_zero->get_data_length() == 0,
            "reg2bus handles n_bits == 0 without underflow");

      // Null & wrong type error guards
      reg_tlm_msg_catcher catcher;
      uvm_report_cb::add(nullptr, &catcher);

      uvm_reg_bus_op rw_err;
      adapter->bus2reg(nullptr, rw_err);
      check(catcher.null_item_fatal == 1 && rw_err.status == UVM_NOT_OK,
            "bus2reg(nullptr) emits REG/NULL_ITEM fatal and sets UVM_NOT_OK");

      dummy_seq_item wrong_item("wrong_item");
      adapter->bus2reg(&wrong_item, rw_err);
      check(catcher.wrong_type_error == 1 && rw_err.status == UVM_NOT_OK,
            "bus2reg(wrong type) emits WRONG_TYPE error and sets UVM_NOT_OK");

      uvm_report_cb::do_delete(nullptr, &catcher);
    }

    // 2. End-to-end RAL frontdoor + field lane + 64b multi-beat + mem + predictor
    {
      uvm_status_e status = UVM_NOT_OK;
      uvm_reg_data_t rdata = 0;

      // Write whole 32-bit register
      blk->r32->write(status, 0xAA00BB11ULL);
      check(status == UVM_IS_OK, "r32 write ok");
      blk->r32->read(status, rdata);
      check(status == UVM_IS_OK && rdata == 0xAA00BB11ULL, "r32 read back 0xAA00BB11");

      // Write individual 8-bit field at lsb=16 (byte lane 2)
      drv->observed.clear();
      blk->r32->f_mid->write(status, 0x5AULL);
      check(status == UVM_IS_OK, "f_mid write ok");
      check(drv->observed.size() == 1 &&
            drv->observed[0].get_data_length() >= 3 &&
            drv->observed[0].m_byte_enable[0] == UVM_TLM_BYTE_DISABLED &&
            drv->observed[0].m_byte_enable[1] == UVM_TLM_BYTE_DISABLED &&
            drv->observed[0].m_byte_enable[2] == UVM_TLM_BYTE_ENABLED,
            "f_mid field write enables byte lane 2");

      blk->r32->read(status, rdata);
      check(status == UVM_IS_OK && rdata == 0xAA5ABB11ULL,
            "f_mid write preserved adjacent fields (0xAA5ABB11)");

      // Mirror check via predictor
      blk->r32->mirror(status, UVM_CHECK);
      check(status == UVM_IS_OK, "r32 mirror UVM_CHECK ok");

      // 64-bit multi-beat register over 32-bit bus
      blk->r64->write(status, 0x1122334455667788ULL);
      check(status == UVM_IS_OK, "r64 multi-beat write ok");
      blk->r64->read(status, rdata);
      check(status == UVM_IS_OK && rdata == 0x1122334455667788ULL,
            "r64 multi-beat read back 0x1122334455667788");

      // uvm_mem read/write
      blk->mem->write(status, 3, 0xCAFEBABEULL);
      check(status == UVM_IS_OK, "uvm_mem write ok");
      blk->mem->read(status, 3, rdata);
      check(status == UVM_IS_OK && rdata == 0xCAFEBABEULL, "uvm_mem read back 0xCAFEBABE");
    }

    std::cout << "tlm_adapter_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
    phase.drop_objection(this);
  }

  virtual void report_phase(uvm_phase& phase)
  {
    uvm_test::report_phase(phase);
    uvm_report_server* svr = uvm_coreservice_t::get()->get_report_server();
    if (all_ok &&
        svr->get_severity_count(UVM_WARNING) == 0 &&
        svr->get_severity_count(UVM_ERROR) == 0 &&
        svr->get_severity_count(UVM_FATAL) == 0)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "!! UVM TEST FAILED !!" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("tlm_adapter_test");
  return 0;
}
