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

// Verify register, field, memory and block sampling hooks across:
// 1. Single-word uvm_mem frontdoor write/read (exact 1 sample, not off-by-one 2)
// 2. Individually accessible uvm_reg_field frontdoor write/read with auto_predict
// 3. Explicit uvm_reg_predictor observation of memory bus transactions

#include <iostream>
#include <map>
#include <vector>

#include <systemc>
#include <uvm>

using namespace uvm;

struct bus_tx : public uvm_sequence_item {
  bool is_read{false};
  uvm_reg_addr_t addr{0};
  uvm_reg_data_t data{0};
  uvm_reg_byte_en_t byte_en{-1};

  UVM_OBJECT_UTILS(bus_tx);

  bus_tx(const std::string& name = "bus_tx") : uvm_sequence_item(name) {}
};

class bus_adapter : public uvm_reg_adapter {
public:
  UVM_OBJECT_UTILS(bus_adapter);

  bus_adapter(const std::string& name = "bus_adapter") : uvm_reg_adapter(name) {
    supports_byte_enable = true;
    provides_responses = true;
  }

  virtual uvm_sequence_item* reg2bus(const uvm_reg_bus_op& rw) {
    bus_tx* tx = bus_tx::type_id::create("tx");
    tx->is_read = (rw.kind == UVM_READ);
    tx->addr = rw.addr;
    tx->data = rw.data;
    tx->byte_en = rw.byte_en;
    return tx;
  }

  virtual void bus2reg(const uvm_sequence_item* bus_item, uvm_reg_bus_op& rw) {
    const bus_tx* tx = dynamic_cast<const bus_tx*>(bus_item);
    if (!tx) UVM_FATAL("ADAPT", "bad item");
    rw.kind = tx->is_read ? UVM_READ : UVM_WRITE;
    rw.addr = tx->addr;
    rw.data = tx->data;
    rw.byte_en = tx->byte_en;
    rw.status = UVM_IS_OK;
  }
};

class bus_driver : public uvm_driver<bus_tx> {
public:
  UVM_COMPONENT_UTILS(bus_driver);

  std::map<uvm_reg_addr_t, uint64_t> storage;

  bus_driver(uvm_component_name name) : uvm_driver<bus_tx>(name) {}

  void run_phase(uvm_phase&) {
    bus_tx req;
    while (true) {
      seq_item_port.get_next_item(req);
      bus_tx rsp("rsp");
      rsp.set_id_info(req);
      rsp.is_read = req.is_read;
      rsp.addr = req.addr;
      rsp.byte_en = req.byte_en;
      if (req.is_read) {
        rsp.data = storage[req.addr];
      } else {
        storage[req.addr] = req.data.to_uint64();
        rsp.data = req.data;
      }
      seq_item_port.item_done(rsp);
    }
  }
};

class sampled_reg : public uvm_reg {
public:
  uvm_reg_field* lo{nullptr};
  uvm_reg_field* hi{nullptr};
  int sample_count{0};
  uint64_t last_data{0};
  uint64_t last_be{0};
  bool last_is_read{false};

  UVM_OBJECT_UTILS(sampled_reg);

  sampled_reg(const std::string& name = "sampled_reg")
      : uvm_reg(name, 16, UVM_CVR_ALL) {}

  void build() {
    lo = uvm_reg_field::type_id::create("lo");
    lo->configure(this, 8, 0, "RW", false, 0x11, true, true, true);
    hi = uvm_reg_field::type_id::create("hi");
    hi->configure(this, 8, 8, "RW", false, 0x22, true, true, true);
  }

  virtual void sample(uvm_reg_data_t data,
                      uvm_reg_data_t byte_en,
                      bool is_read,
                      uvm_reg_map*) {
    sample_count++;
    last_data = data.to_uint64();
    last_be = byte_en.to_uint64();
    last_is_read = is_read;
  }
};

class sampled_mem : public uvm_mem {
public:
  std::vector<std::pair<uvm_reg_addr_t, bool>> samples;

  sampled_mem(const std::string& name = "sampled_mem")
      : uvm_mem(name, 4, 8, "RW", UVM_CVR_ALL) {}

  virtual void sample(uvm_reg_addr_t offset, bool is_read, uvm_reg_map*) {
    samples.push_back(std::make_pair(offset, is_read));
  }
};

class sampled_block : public uvm_reg_block {
public:
  sampled_reg* r0{nullptr};
  sampled_mem* mem{nullptr};
  std::vector<std::pair<uvm_reg_addr_t, bool>> addr_samples;

  UVM_OBJECT_UTILS(sampled_block);

  sampled_block(const std::string& name = "blk")
      : uvm_reg_block(name, UVM_CVR_ALL) {}

  void build() {
    // 1-byte bus so 8-bit fields lo and hi are individually accessible
    default_map = create_map("default_map", 0, 1, UVM_LITTLE_ENDIAN, true);

    r0 = new sampled_reg("r0");
    r0->configure(this);
    r0->build();
    r0->set_coverage(UVM_CVR_ALL);
    default_map->add_reg(r0, 0x0);

    mem = new sampled_mem("mem");
    mem->configure(this);
    mem->set_coverage(UVM_CVR_ALL);
    default_map->add_mem(mem, 0x10);

    set_coverage(UVM_CVR_ALL);
    lock_model();
  }

  virtual void sample(uvm_reg_addr_t offset, bool is_read, uvm_reg_map*) {
    addr_samples.push_back(std::make_pair(offset, is_read));
  }
};

class sample_test : public uvm_test {
public:
  UVM_COMPONENT_UTILS(sample_test);

  sampled_block* blk{nullptr};
  uvm_sequencer<bus_tx>* sqr{nullptr};
  bus_driver* drv{nullptr};
  bus_adapter* adapt{nullptr};
  uvm_reg_predictor<bus_tx>* pred{nullptr};

  sample_test(uvm_component_name name) : uvm_test(name) {}

  void build_phase(uvm_phase& phase) {
    uvm_test::build_phase(phase);
    blk = new sampled_block("blk");
    blk->build();
    blk->reset();

    sqr = new uvm_sequencer<bus_tx>("sqr");
    drv = bus_driver::type_id::create("drv", this);
    adapt = bus_adapter::type_id::create("adapt");
    pred = new uvm_reg_predictor<bus_tx>("pred");
  }

  void connect_phase(uvm_phase&) {
    drv->seq_item_port.connect(sqr->seq_item_export);
    blk->default_map->set_sequencer(sqr, adapt);
    blk->default_map->set_auto_predict(true);
    pred->map = blk->default_map;
    pred->adapter = adapt;
  }

  void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);

    uvm_status_e status;
    uvm_reg_data_t val = 0;

    // 1. Individually accessible field write & read on hi [15:8]
    blk->r0->hi->write(status, 0xab);
    std::cout << "field write: reg_samples=" << blk->r0->sample_count
              << " data=0x" << std::hex << blk->r0->last_data
              << " be=0x" << blk->r0->last_be << std::dec
              << " is_read=" << blk->r0->last_is_read
              << " blk_samples=" << blk->addr_samples.size() << std::endl;

    blk->r0->hi->read(status, val);
    std::cout << "field read: reg_samples=" << blk->r0->sample_count
              << " val=0x" << std::hex << val.to_uint64()
              << " data=0x" << blk->r0->last_data
              << " be=0x" << blk->r0->last_be << std::dec
              << " is_read=" << blk->r0->last_is_read
              << " blk_samples=" << blk->addr_samples.size() << std::endl;

    // 2. Single-word memory write & read at offset 3 (last word)
    blk->mem->write(status, 3, 0x5a);
    blk->mem->read(status, 3, val);
    std::cout << "mem frontdoor samples=" << blk->mem->samples.size()
              << " first_off=" << (blk->mem->samples.empty() ? 99ULL : blk->mem->samples[0].first.to_uint64())
              << " second_off=" << (blk->mem->samples.size() < 2 ? 99ULL : blk->mem->samples[1].first.to_uint64())
              << std::endl;

    // 3. Explicit predictor observation of autonomous bus write & read to mem[2] (addr 0x12)
    size_t mem_before = blk->mem->samples.size();
    size_t blk_before = blk->addr_samples.size();
    bus_tx obs_wr("obs_wr");
    obs_wr.is_read = false;
    obs_wr.addr = 0x12;
    obs_wr.data = 0x77;
    pred->write(obs_wr);

    bus_tx obs_rd("obs_rd");
    obs_rd.is_read = true;
    obs_rd.addr = 0x12;
    obs_rd.data = 0x77;
    pred->write(obs_rd);

    std::cout << "predictor mem_delta=" << (blk->mem->samples.size() - mem_before)
              << " blk_delta=" << (blk->addr_samples.size() - blk_before);
    if (blk->mem->samples.size() >= mem_before + 2) {
      std::cout << " pred_off=" << blk->mem->samples[mem_before].first
                << " pred_wr_is_read=" << blk->mem->samples[mem_before].second
                << " pred_rd_is_read=" << blk->mem->samples[mem_before + 1].second;
    }
    std::cout << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  run_test("sample_test");
  return 0;
}
