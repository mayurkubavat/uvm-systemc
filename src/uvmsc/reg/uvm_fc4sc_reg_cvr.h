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

#ifndef UVM_FC4SC_REG_CVR_H_
#define UVM_FC4SC_REG_CVR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <fc4sc.hpp>

#include "uvmsc/reg/uvm_mem.h"
#include "uvmsc/reg/uvm_reg.h"
#include "uvmsc/reg/uvm_reg_block.h"
#include "uvmsc/reg/uvm_reg_field.h"
#include "uvmsc/reg/uvm_reg_map.h"
#include "uvmsc/reg/uvm_reg_model.h"

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_fc4sc_reg
//
// Register base class that automatically constructs and samples FC4SC
// dynamic covergroups for UVM_CVR_REG_BITS and UVM_CVR_FIELD_VALS from
// configured uvm_reg_field metadata when included via
// uvm_reg::include_coverage().
//----------------------------------------------------------------------

class uvm_fc4sc_reg : public uvm_reg {
public:
  explicit uvm_fc4sc_reg(const std::string& name,
                         unsigned int n_bits,
                         int has_cov = UVM_NO_COVERAGE)
      : uvm_reg(name, n_bits, has_cov) {}

  virtual ~uvm_fc4sc_reg() = default;

  void build_fc4sc_coverage(fc4sc::global* cntxt = fc4sc::global::getter()) {
    if (m_fc4sc_built) return;
    m_fc4sc_built = true;

    uvm_reg_cvr_t inc = build_coverage(UVM_CVR_REG_BITS | UVM_CVR_FIELD_VALS);
    add_coverage(inc);

    std::vector<uvm_reg_field*> fields;
    get_fields(fields);
    if (fields.empty()) return;

    if (has_coverage(UVM_CVR_REG_BITS)) {
      m_bits_type_name = get_full_name() + "_reg_bits_cg";
      m_bits_factory.reset(
          new fc4sc::dynamic_covergroup_factory(m_bits_type_name.c_str(), __FILE__, __LINE__));

      struct bit_binder {
        unsigned int bit;
        bool is_read_cvp;
        fc4sc::dynamic_coverpoint_factory<int(uint64_t), bool(int, uint64_t)> cvp;
      };
      std::vector<bit_binder> binders;

      for (auto* f : fields) {
        const std::string acc = f->get_access();
        const bool can_write =
            (acc != "RO" && acc != "NOACCESS" && acc != "RC" && acc != "RS");
        const bool can_read = (acc != "WO" && acc != "WO1" && acc != "NOACCESS");
        const unsigned int lsb = f->get_lsb_pos();
        const unsigned int sz = f->get_n_bits();

        for (unsigned int i = 0; i < sz && (lsb + i) < 64; ++i) {
          const unsigned int bit = lsb + i;
          const unsigned int byte_idx = bit / 8;

          if (can_write) {
            auto cvp = m_bits_factory->create_coverpoint<int(uint64_t), bool(int, uint64_t)>(
                f->get_name() + "_bit" + std::to_string(i) + "_wr",
                [bit](uint64_t d) -> int { return static_cast<int>((d >> bit) & 1ULL); },
                [byte_idx](int rd, uint64_t be) -> bool {
                  return rd == 0 && (((be >> byte_idx) & 1ULL) != 0ULL);
                });
            cvp.create_bin("zero", 0);
            cvp.create_bin("one", 1);
            binders.push_back(bit_binder{bit, false, cvp});
          }

          if (can_read) {
            auto cvp = m_bits_factory->create_coverpoint<int(uint64_t), bool(int, uint64_t)>(
                f->get_name() + "_bit" + std::to_string(i) + "_rd",
                [bit](uint64_t d) -> int { return static_cast<int>((d >> bit) & 1ULL); },
                [byte_idx](int rd, uint64_t be) -> bool {
                  return rd != 0 && (((be >> byte_idx) & 1ULL) != 0ULL);
                });
            cvp.create_bin("zero", 0);
            cvp.create_bin("one", 1);
            binders.push_back(bit_binder{bit, true, cvp});
          }
        }
      }

      if (!binders.empty()) {
        m_bits_cg.reset(new fc4sc::dynamic_covergroup(
            *m_bits_factory, get_full_name() + ".reg_bits", __FILE__, __LINE__, cntxt));
        m_bits_cg->option().per_instance = true;
        for (auto& b : binders) {
          b.cvp.bind_sample(*m_bits_cg, m_sample_data);
          b.cvp.bind_condition(*m_bits_cg, m_sample_is_read, m_sample_byte_en);
        }
      }
    }

    if (has_coverage(UVM_CVR_FIELD_VALS)) {
      m_vals_type_name = get_full_name() + "_field_vals_cg";
      m_vals_factory.reset(
          new fc4sc::dynamic_covergroup_factory(m_vals_type_name.c_str(), __FILE__, __LINE__));

      m_field_vals.assign(fields.size(), 0ULL);
      std::vector<fc4sc::dynamic_coverpoint_factory<uint64_t(uint64_t), bool()>> val_cvps;

      for (size_t idx = 0; idx < fields.size(); ++idx) {
        uvm_reg_field* f = fields[idx];
        const unsigned int sz = f->get_n_bits();
        if (sz == 0 || sz > 64) continue;
        const uint64_t max_val = (sz == 64) ? ~uint64_t(0) : ((uint64_t(1) << sz) - 1ULL);

        auto cvp = m_vals_factory->create_coverpoint<uint64_t(uint64_t)>(
            f->get_name(), [](uint64_t v) -> uint64_t { return v; });

        if (sz <= 3) {
          for (uint64_t v = 0; v <= max_val; ++v) {
            cvp.create_bin("val_" + std::to_string(v), v);
          }
        } else {
          const uint64_t mid = max_val / 2ULL;
          cvp.create_bin("zero", uint64_t(0));
          cvp.create_bin("low", fc4sc::interval<uint64_t>(1ULL, mid));
          if (max_val > 2ULL && mid + 1ULL <= max_val - 1ULL) {
            cvp.create_bin("high", fc4sc::interval<uint64_t>(mid + 1ULL, max_val - 1ULL));
          }
          cvp.create_bin("all_ones", max_val);
        }
        val_cvps.push_back(cvp);
      }

      if (!val_cvps.empty()) {
        m_vals_cg.reset(new fc4sc::dynamic_covergroup(
            *m_vals_factory, get_full_name() + ".field_vals", __FILE__, __LINE__, cntxt));
        m_vals_cg->option().per_instance = true;
        size_t cvp_idx = 0;
        for (size_t idx = 0; idx < fields.size(); ++idx) {
          const unsigned int sz = fields[idx]->get_n_bits();
          if (sz == 0 || sz > 64) continue;
          val_cvps[cvp_idx++].bind_sample(*m_vals_cg, m_field_vals[idx]);
        }
      }
    }
  }

  virtual void sample(uvm_reg_data_t data,
                      uvm_reg_data_t byte_en,
                      bool is_read,
                      uvm_reg_map* map) override {
    uvm_reg::sample(data, byte_en, is_read, map);
    if (!m_fc4sc_built) build_fc4sc_coverage();
    if (m_bits_cg && get_coverage(UVM_CVR_REG_BITS)) {
      m_sample_data = data.to_uint64();
      m_sample_byte_en = byte_en.to_uint64();
      m_sample_is_read = is_read ? 1 : 0;
      m_bits_cg->sample();
    }
  }

  virtual void sample_values() override {
    uvm_reg::sample_values();
    if (!m_fc4sc_built) build_fc4sc_coverage();
    if (m_vals_cg && get_coverage(UVM_CVR_FIELD_VALS)) {
      std::vector<uvm_reg_field*> fields;
      get_fields(fields);
      for (size_t i = 0; i < fields.size() && i < m_field_vals.size(); ++i) {
        m_field_vals[i] = fields[i]->get_mirrored_value().to_uint64();
      }
      m_vals_cg->sample();
    }
  }

  double get_reg_bits_coverage() {
    return m_bits_cg ? m_bits_cg->get_inst_coverage() : 0.0;
  }

  double get_field_vals_coverage() {
    return m_vals_cg ? m_vals_cg->get_inst_coverage() : 0.0;
  }

private:
  bool m_fc4sc_built{false};
  uint64_t m_sample_data{0};
  uint64_t m_sample_byte_en{~uint64_t(0)};
  int m_sample_is_read{0};
  std::vector<uint64_t> m_field_vals;

  std::string m_bits_type_name;
  std::string m_vals_type_name;
  std::unique_ptr<fc4sc::dynamic_covergroup_factory> m_bits_factory;
  std::unique_ptr<fc4sc::dynamic_covergroup_factory> m_vals_factory;
  std::unique_ptr<fc4sc::dynamic_covergroup> m_bits_cg;
  std::unique_ptr<fc4sc::dynamic_covergroup> m_vals_cg;
};

//----------------------------------------------------------------------
// Class: uvm_fc4sc_reg_block
//
// Block base class that automatically builds an FC4SC address-map
// covergroup (UVM_CVR_ADDR_MAP) over mapped registers and memories at
// lock_model() time and triggers child uvm_fc4sc_reg coverage setup.
//----------------------------------------------------------------------

class uvm_fc4sc_reg_block : public uvm_reg_block {
public:
  explicit uvm_fc4sc_reg_block(const std::string& name = "",
                               int has_cov = UVM_NO_COVERAGE)
      : uvm_reg_block(name, has_cov) {}

  virtual ~uvm_fc4sc_reg_block() = default;

  virtual void lock_model() override {
    uvm_reg_block::lock_model();
    build_fc4sc_coverage();
  }

  void build_fc4sc_coverage(fc4sc::global* cntxt = fc4sc::global::getter()) {
    if (m_fc4sc_built) return;
    m_fc4sc_built = true;

    uvm_reg_cvr_t inc = build_coverage(UVM_CVR_ADDR_MAP);
    add_coverage(inc);

    std::vector<uvm_reg*> regs;
    get_registers(regs, UVM_NO_HIER);
    for (auto* r : regs) {
      if (auto* fr = dynamic_cast<uvm_fc4sc_reg*>(r)) {
        fr->build_fc4sc_coverage(cntxt);
      }
    }

    std::vector<uvm_reg_block*> blks;
    get_blocks(blks, UVM_NO_HIER);
    for (auto* b : blks) {
      if (auto* fb = dynamic_cast<uvm_fc4sc_reg_block*>(b)) {
        fb->build_fc4sc_coverage(cntxt);
      }
    }

    if (!has_coverage(UVM_CVR_ADDR_MAP)) return;

    std::vector<uvm_reg_map*> maps;
    get_maps(maps);
    if (maps.empty()) return;

    m_addr_type_name = get_full_name() + "_addr_map_cg";
    m_addr_factory.reset(
        new fc4sc::dynamic_covergroup_factory(m_addr_type_name.c_str(), __FILE__, __LINE__));

    bool has_wr = false;
    bool has_rd = false;

    uvm_reg_map* primary = maps.front();
    std::vector<uvm_reg*> map_regs;
    primary->get_registers(map_regs, UVM_NO_HIER);
    std::vector<uvm_mem*> map_mems;
    primary->get_memories(map_mems, UVM_NO_HIER);

    auto reg_can_write = [](uvm_reg* r, uvm_reg_map* m) -> bool {
      if (r->get_rights(m) == "RO") return false;
      std::vector<uvm_reg_field*> fs;
      r->get_fields(fs);
      if (fs.empty()) return true;
      for (auto* f : fs) {
        const std::string a = f->get_access(m);
        if (a != "RO" && a != "NOACCESS" && a != "RC" && a != "RS") return true;
      }
      return false;
    };

    auto reg_can_read = [](uvm_reg* r, uvm_reg_map* m) -> bool {
      if (r->get_rights(m) == "WO") return false;
      std::vector<uvm_reg_field*> fs;
      r->get_fields(fs);
      if (fs.empty()) return true;
      for (auto* f : fs) {
        const std::string a = f->get_access(m);
        if (a != "WO" && a != "WO1" && a != "NOACCESS") return true;
      }
      return false;
    };

    for (auto* r : map_regs) {
      if (reg_can_write(r, primary)) has_wr = true;
      if (reg_can_read(r, primary)) has_rd = true;
    }
    for (auto* m : map_mems) {
      const std::string acc = m->get_access(primary);
      if (acc != "RO") has_wr = true;
      if (acc != "WO") has_rd = true;
    }

    fc4sc::dynamic_coverpoint_factory<uint64_t(uint64_t), bool(int)> wr_cvp;
    fc4sc::dynamic_coverpoint_factory<uint64_t(uint64_t), bool(int)> rd_cvp;

    if (has_wr) {
      wr_cvp = m_addr_factory->create_coverpoint<uint64_t(uint64_t), bool(int)>(
          "addr_wr",
          [](uint64_t a) -> uint64_t { return a; },
          [](int is_rd) -> bool { return is_rd == 0; });
      for (auto* r : map_regs) {
        if (!reg_can_write(r, primary)) continue;
        wr_cvp.create_bin("reg_" + r->get_name(), r->get_offset(primary).to_uint64());
      }
      for (auto* m : map_mems) {
        if (m->get_access(primary) == "RO") continue;
        uvm_reg_map_info* info = primary->get_mem_map_info(m);
        wr_cvp.create_bin("mem_" + m->get_name(),
                          fc4sc::interval<uint64_t>(info->mem_range.min.to_uint64(),
                                                    info->mem_range.max.to_uint64()));
      }
    }

    if (has_rd) {
      rd_cvp = m_addr_factory->create_coverpoint<uint64_t(uint64_t), bool(int)>(
          "addr_rd",
          [](uint64_t a) -> uint64_t { return a; },
          [](int is_rd) -> bool { return is_rd != 0; });
      for (auto* r : map_regs) {
        if (!reg_can_read(r, primary)) continue;
        rd_cvp.create_bin("reg_" + r->get_name(), r->get_offset(primary).to_uint64());
      }
      for (auto* m : map_mems) {
        if (m->get_access(primary) == "WO") continue;
        uvm_reg_map_info* info = primary->get_mem_map_info(m);
        rd_cvp.create_bin("mem_" + m->get_name(),
                          fc4sc::interval<uint64_t>(info->mem_range.min.to_uint64(),
                                                    info->mem_range.max.to_uint64()));
      }
    }

    if (has_wr || has_rd) {
      m_addr_cg.reset(new fc4sc::dynamic_covergroup(
          *m_addr_factory, get_full_name() + ".addr_map", __FILE__, __LINE__, cntxt));
      m_addr_cg->option().per_instance = true;
      if (has_wr) {
        wr_cvp.bind_sample(*m_addr_cg, m_sample_addr);
        wr_cvp.bind_condition(*m_addr_cg, m_sample_is_read);
      }
      if (has_rd) {
        rd_cvp.bind_sample(*m_addr_cg, m_sample_addr);
        rd_cvp.bind_condition(*m_addr_cg, m_sample_is_read);
      }
    }
  }

  void enable_all_coverage(uvm_reg_cvr_t models = UVM_CVR_ALL) {
    set_coverage(models);
    std::vector<uvm_reg*> regs;
    get_registers(regs, UVM_HIER);
    for (auto* r : regs) r->set_coverage(models);
    std::vector<uvm_mem*> mems;
    get_memories(mems, UVM_HIER);
    for (auto* m : mems) m->set_coverage(models);
    std::vector<uvm_reg_block*> blks;
    get_blocks(blks, UVM_HIER);
    for (auto* b : blks) b->set_coverage(models);
  }

  virtual void sample(uvm_reg_addr_t offset,
                      bool is_read,
                      uvm_reg_map* map) override {
    uvm_reg_block::sample(offset, is_read, map);
    if (!m_fc4sc_built) build_fc4sc_coverage();
    if (m_addr_cg && get_coverage(UVM_CVR_ADDR_MAP)) {
      m_sample_addr = offset.to_uint64();
      m_sample_is_read = is_read ? 1 : 0;
      m_addr_cg->sample();
    }
  }

  double get_addr_map_coverage() {
    return m_addr_cg ? m_addr_cg->get_inst_coverage() : 0.0;
  }

private:
  bool m_fc4sc_built{false};
  uint64_t m_sample_addr{0};
  int m_sample_is_read{0};
  std::string m_addr_type_name;
  std::unique_ptr<fc4sc::dynamic_covergroup_factory> m_addr_factory;
  std::unique_ptr<fc4sc::dynamic_covergroup> m_addr_cg;
};

} // namespace uvm

#endif // UVM_FC4SC_REG_CVR_H_
