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

// HDL backdoor access on Verilator-style SystemC ports/signals (single-writer
// sc_signal<T>, sc_in<T>/sc_out<T>, integral C++ types, and bit/range slices)
// plus typed uvm_reg_backdoor adapters for plain C++ RTL variables (modeling
// Verilator rootp public_flat_rw registers) and SystemC signals.

#include <cstdint>
#include <iostream>

#include <systemc>
#include <uvm>

using namespace uvm;

struct fake_verilator_root {
  uint32_t apb_timer__DOT__ctrl_q = 0x00000005u;
  uint32_t apb_timer__DOT__count_q = 0x00001234u;
};

class verilated_like_dut : public sc_core::sc_module {
 public:
  sc_core::sc_in<uint32_t> pwdata{"pwdata"};
  sc_core::sc_out<uint32_t> prdata{"prdata"};
  sc_core::sc_out<bool> irq{"irq"};
  fake_verilator_root* rootp;

  explicit verilated_like_dut(sc_core::sc_module_name name)
      : sc_core::sc_module(name), rootp(new fake_verilator_root()) {}

  virtual ~verilated_like_dut() { delete rootp; }
};

class top_tb : public sc_core::sc_module {
 public:
  sc_core::sc_signal<uint32_t> pwdata_sig{"pwdata_sig"};
  sc_core::sc_signal<uint32_t, sc_core::SC_MANY_WRITERS> prdata_sig{"prdata_sig"};
  sc_core::sc_signal<bool> irq_sig{"irq_sig"};
  verilated_like_dut dut{"dut"};

  explicit top_tb(sc_core::sc_module_name name) : sc_core::sc_module(name) {
    dut.pwdata(pwdata_sig);
    dut.prdata(prdata_sig);
    dut.irq(irq_sig);
    pwdata_sig.write(0xCAFEBABEu);
    prdata_sig.write(0xA5A55A5Au);
    irq_sig.write(true);
  }
};

class reg32 : public uvm_reg {
 public:
  uvm_reg_field* data = nullptr;

  explicit reg32(const std::string& name = "reg32")
      : uvm_reg(name, 32, UVM_NO_COVERAGE) {}

  void build(const std::string& access = "RW") {
    data = uvm_reg_field::type_id::create("data");
    data->configure(this, 32, 0, access, false, 0x0, true, false, true);
  }

  UVM_OBJECT_UTILS(reg32);
};

class timer_blk : public uvm_reg_block {
 public:
  reg32* ctrl = nullptr;
  reg32* count = nullptr;
  reg32* prdata_mirror = nullptr;

  explicit timer_blk(const std::string& name = "timer_blk")
      : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  virtual void build() {
    default_map = create_map("default_map", 0x0, 4, UVM_LITTLE_ENDIAN);

    ctrl = reg32::type_id::create("ctrl");
    ctrl->configure(this, nullptr);
    ctrl->build("RW");
    default_map->add_reg(ctrl, 0x0, "RW");

    count = reg32::type_id::create("count");
    count->configure(this, nullptr);
    count->build("RO");
    default_map->add_reg(count, 0x4, "RO");

    prdata_mirror = reg32::type_id::create("prdata_mirror");
    prdata_mirror->configure(this, nullptr);
    prdata_mirror->build("RW");
    default_map->add_reg(prdata_mirror, 0x8, "RW");

    lock_model();
  }

  UVM_OBJECT_UTILS(timer_blk);
};

class test : public uvm_test {
 public:
  top_tb* tb = nullptr;
  timer_blk* model = nullptr;

  UVM_COMPONENT_UTILS(test);

  explicit test(uvm_component_name name) : uvm_test(name) {}

  virtual void build_phase(uvm_phase& phase) {
    uvm_test::build_phase(phase);
    if (!uvm_config_db<top_tb*>::get(this, "", "tb", tb))
      UVM_FATAL("TEST", "Missing top_tb in config_db");

    model = timer_blk::type_id::create("model");
    model->build();
    model->reset();

    uvm_bind_cpp_backdoor(model->ctrl, tb->dut.rootp->apb_timer__DOT__ctrl_q);
    uvm_bind_cpp_backdoor(model->count, tb->dut.rootp->apb_timer__DOT__count_q, false);
    uvm_bind_signal_backdoor(model->prdata_mirror, tb->prdata_sig);
  }

  virtual void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);
    sc_core::wait(sc_core::SC_ZERO_TIME);

    uint32_t v32 = 0;
    bool v1 = false;

    // 1. Read single-writer sc_signal<uint32_t>, sc_in<uint32_t>, sc_out<bool>, and bit slices
    bool ok1 = uvm_hdl_read("top.pwdata_sig", v32);
    std::cout << "read pwdata_sig=" << ok1 << " 0x" << std::hex << v32 << std::dec << std::endl;

    uint32_t slice = 0;
    bool ok2 = uvm_hdl_read("top.dut.pwdata[15:8]", slice);
    std::cout << "read dut.pwdata[15:8]=" << ok2 << " 0x" << std::hex << slice << std::dec << std::endl;

    bool ok3 = uvm_hdl_read("top.dut.irq", v1);
    std::cout << "read dut.irq=" << ok3 << " " << v1 << std::endl;

    // 2. Deposit via sc_out<uint32_t> port bound to SC_MANY_WRITERS signal (including part-select)
    bool ok4 = uvm_hdl_deposit("top.dut.prdata[7:0]", static_cast<uint32_t>(0x3Cu));
    sc_core::wait(sc_core::SC_ZERO_TIME);
    bool ok5 = uvm_hdl_read("top.dut.prdata", v32);
    std::cout << "deposit+read dut.prdata=" << (ok4 && ok5) << " 0x" << std::hex << v32 << std::dec << std::endl;

    // 3. uvm_cpp_reg_backdoor: poke, peek, and mirror(UVM_CHECK, UVM_BACKDOOR)
    uvm_status_e status = UVM_NOT_OK;
    uvm_reg_data_t rval = 0;

    model->ctrl->poke(status, 0x0000BEEFu);
    std::cout << "poke ctrl status=" << (status == UVM_IS_OK)
              << " rtl=0x" << std::hex << tb->dut.rootp->apb_timer__DOT__ctrl_q << std::dec << std::endl;

    tb->dut.rootp->apb_timer__DOT__ctrl_q = 0x13572468u;
    model->ctrl->predict(0x13572468u);
    model->ctrl->mirror(status, UVM_CHECK, UVM_BACKDOOR);
    std::cout << "mirror ctrl status=" << (status == UVM_IS_OK)
              << " mirrored=0x" << std::hex << model->ctrl->get_mirrored_value().to_uint64() << std::dec << std::endl;

    model->count->peek(status, rval);
    std::cout << "peek ro count status=" << (status == UVM_IS_OK)
              << " val=0x" << std::hex << rval.to_uint64() << std::dec << std::endl;

    // 4. uvm_sc_signal_reg_backdoor: backdoor write + peek on SystemC signal
    model->prdata_mirror->poke(status, 0xDEADC0DEu);
    sc_core::wait(sc_core::SC_ZERO_TIME);
    model->prdata_mirror->peek(status, rval);
    std::cout << "signal backdoor status=" << (status == UVM_IS_OK)
              << " val=0x" << std::hex << rval.to_uint64() << std::dec << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  top_tb top("top");
  uvm_config_db<top_tb*>::set(nullptr, "*", "tb", &top);
  run_test("test");
  return 0;
}
