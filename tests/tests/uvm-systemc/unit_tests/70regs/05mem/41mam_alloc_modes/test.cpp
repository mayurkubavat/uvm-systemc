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

// Reproducing test for Fix 4 (uvm_mem_mam):
//   1. reserve_region() stores m_in_use in increasing start_offset order (<)
//   2. request_region() GREEDY+BROAD non-overlapping, exhaustion, deterministic seed
//   3. request_region() THRIFTY best-fit reuse of smallest freed hole
//   4. request_region() NEARBY adjacent allocation
//   5. uvm_mem_mam_policy pre_randomize()/post_randomize() invocation

#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <systemc>
#include <uvm>

using namespace uvm;

class err_catcher : public uvm_report_catcher {
 public:
  int caught = 0;

  err_catcher() : uvm_report_catcher("err_catcher") {}

  virtual action_e do_catch() {
    if (get_severity() == UVM_ERROR && get_id() == "RegModel") {
      ++caught;
      return CAUGHT;
    }
    return THROW;
  }
};

class custom_policy : public uvm_mem_mam_policy {
 public:
  int pre_calls = 0;
  int post_calls = 0;
  unsigned long seen_min = 0;
  unsigned long seen_max = 0;
  unsigned int seen_len = 0;

  virtual void pre_randomize() {
    ++pre_calls;
    seen_min = min_offset;
    seen_max = max_offset;
    seen_len = len;
  }

  virtual void post_randomize() {
    ++post_calls;
  }
};

class test : public uvm_test {
 public:
  UVM_COMPONENT_UTILS(test);

  int fails = 0;

  explicit test(uvm_component_name name) : uvm_test(name) {}

  virtual void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);

    // Suppress UVM_MEDIUM [RegModel] reservation info logs so golden/test.log
    // does not depend on std::uniform_int_distribution output formatting,
    // while keeping UVM_ERROR (UVM_NONE) intact for err_catcher checks.
    uvm_root::get()->set_report_id_verbosity("RegModel", UVM_LOW);

    // 1. reserve_region() increasing order
    {
      uvm_mem_mam_cfg cfg;
      cfg.n_bytes = 1;
      cfg.start_offset = 0x0;
      cfg.end_offset = 0x3ff;
      cfg.mode = uvm_mem_mam::GREEDY;
      cfg.locality = uvm_mem_mam::BROAD;

      uvm_mem_mam mam("mam_order", &cfg);
      mam.reserve_region(0x200, 0x20);
      mam.reserve_region(0x10, 0x10);
      mam.reserve_region(0x100, 0x20);

      std::vector<unsigned long> offsets;
      for (uvm_mem_region* r = mam.for_each(true); r != nullptr; r = mam.for_each(false)) {
        offsets.push_back(r->get_start_offset());
      }
      bool ord_ok = (offsets.size() == 3 &&
                     offsets[0] == 0x10 && offsets[1] == 0x100 && offsets[2] == 0x200);
      std::cout << "order_ok=" << ord_ok << std::endl;
      if (!ord_ok) ++fails;
    }

    // 2. GREEDY + BROAD: non-overlapping allocations + exhaustion + determinism
    {
      uvm_mem_mam_cfg cfg;
      cfg.n_bytes = 1;
      cfg.start_offset = 0x0;
      cfg.end_offset = 0xff;
      cfg.mode = uvm_mem_mam::GREEDY;
      cfg.locality = uvm_mem_mam::BROAD;

      uvm_mem_mam mam1("mam_det", &cfg);
      std::vector<unsigned long> s1;
      for (int i = 0; i < 4; ++i) {
        uvm_mem_region* r = mam1.request_region(0x10);
        if (r != nullptr) s1.push_back(r->get_start_offset());
      }

      uvm_mem_mam mam2("mam_det", &cfg);
      std::vector<unsigned long> s2;
      for (int i = 0; i < 4; ++i) {
        uvm_mem_region* r = mam2.request_region(0x10);
        if (r != nullptr) s2.push_back(r->get_start_offset());
      }

      uvm_mem_mam_cfg small_cfg;
      small_cfg.n_bytes = 1;
      small_cfg.start_offset = 0x0;
      small_cfg.end_offset = 0x2f;
      small_cfg.mode = uvm_mem_mam::GREEDY;
      small_cfg.locality = uvm_mem_mam::BROAD;
      uvm_mem_mam mam_small("mam_small", &small_cfg);
      uvm_mem_region* first_r = mam_small.request_region(0x20);
      err_catcher catcher;
      uvm_report_cb::add(nullptr, &catcher);
      uvm_mem_region* full_r = mam_small.request_region(0x20);
      uvm_report_cb::do_delete(nullptr, &catcher);

      bool greedy_ok = (s1.size() == 4 && s1 == s2 &&
                        first_r != nullptr && full_r == nullptr && catcher.caught == 1);
      std::cout << "greedy_ok=" << greedy_ok << " count=" << s1.size()
                << " full_null=" << (full_r == nullptr) << std::endl;
      if (!greedy_ok) ++fails;
    }

    // 3. THRIFTY: reuses smallest fitting freed hole
    {
      uvm_mem_mam_cfg cfg;
      cfg.n_bytes = 1;
      cfg.start_offset = 0x0;
      cfg.end_offset = 0x7f;
      cfg.mode = uvm_mem_mam::THRIFTY;
      cfg.locality = uvm_mem_mam::BROAD;

      uvm_mem_mam mam("mam_thrifty", &cfg);
      uvm_mem_region* r0 = mam.reserve_region(0x00, 0x20);
      uvm_mem_region* r1 = mam.reserve_region(0x20, 0x10);
      uvm_mem_region* r2 = mam.reserve_region(0x30, 0x20);
      uvm_mem_region* r3 = mam.reserve_region(0x50, 0x30);
      (void)r0;
      (void)r2;
      mam.release_region(r1); // 0x10 hole at [0x20:0x2f]
      mam.release_region(r3); // 0x30 hole at [0x50:0x7f]

      uvm_mem_region* picked = mam.request_region(0x10);
      bool thrifty_ok = (picked != nullptr && picked->get_start_offset() == 0x20 &&
                         picked->get_end_offset() == 0x2f);
      std::cout << "thrifty_ok=" << thrifty_ok
                << " start=0x" << std::hex << (picked ? picked->get_start_offset() : 0xFFFF)
                << std::dec << std::endl;
      if (!thrifty_ok) ++fails;
    }

    // 4. NEARBY: allocates adjacent to existing region
    {
      uvm_mem_mam_cfg cfg;
      cfg.n_bytes = 1;
      cfg.start_offset = 0x0;
      cfg.end_offset = 0x3ff;
      cfg.mode = uvm_mem_mam::GREEDY;
      cfg.locality = uvm_mem_mam::NEARBY;

      uvm_mem_mam mam("mam_nearby", &cfg);
      mam.reserve_region(0x100, 0x10); // [0x100:0x10f]
      uvm_mem_region* adj = mam.request_region(0x10);
      bool nearby_ok = (adj != nullptr &&
                        (adj->get_start_offset() == 0x110 || adj->get_end_offset() == 0xff));
      std::cout << "nearby_ok=" << nearby_ok << std::endl;
      if (!nearby_ok) ++fails;
    }

    // 5. Custom policy hooks
    {
      uvm_mem_mam_cfg cfg;
      cfg.n_bytes = 4;
      cfg.start_offset = 0x10;
      cfg.end_offset = 0x3f;
      cfg.mode = uvm_mem_mam::GREEDY;
      cfg.locality = uvm_mem_mam::BROAD;

      uvm_mem_mam mam("mam_pol", &cfg);
      custom_policy pol;
      uvm_mem_region* r = mam.request_region(16, &pol, __FILE__, __LINE__);
      bool pol_ok = (r != nullptr && pol.pre_calls == 1 && pol.post_calls == 1 &&
                     pol.seen_min == 0x10 && pol.seen_max == 0x3f && pol.seen_len == 4);
      std::cout << "policy_ok=" << pol_ok << std::endl;
      if (!pol_ok) ++fails;
    }

    // 6. Full unsigned long [0, ULONG_MAX] address space & unsigned int n_bytes overflow guard
    {
      uvm_mem_mam_cfg full_cfg;
      full_cfg.n_bytes = 1;
      full_cfg.start_offset = 0x0;
      full_cfg.end_offset = std::numeric_limits<unsigned long>::max();
      full_cfg.mode = uvm_mem_mam::GREEDY;
      full_cfg.locality = uvm_mem_mam::BROAD;

      uvm_mem_mam mam_full("mam_full", &full_cfg);
      uvm_mem_region* r_full = mam_full.request_region(16);
      bool full_space_ok = (r_full != nullptr && r_full->get_n_bytes() == 16);
      std::cout << "full_space_ok=" << full_space_ok << std::endl;
      if (!full_space_ok) ++fails;

      uvm_mem_mam_cfg wide_cfg;
      wide_cfg.n_bytes = 4;
      wide_cfg.start_offset = 0x0;
      wide_cfg.end_offset = 0xFFFFFFFFUL;
      wide_cfg.mode = uvm_mem_mam::GREEDY;
      wide_cfg.locality = uvm_mem_mam::BROAD;

      uvm_mem_mam mam_ovf("mam_ovf", &wide_cfg);
      err_catcher catcher;
      uvm_report_cb::add(nullptr, &catcher);
      uvm_mem_region* r_res_ovf =
          mam_ovf.reserve_region(0, std::numeric_limits<unsigned int>::max());
      uvm_mem_region* r_req_ovf =
          mam_ovf.request_region(std::numeric_limits<unsigned int>::max());
      uvm_report_cb::do_delete(nullptr, &catcher);

      bool ovf_ok = (r_res_ovf == nullptr && r_req_ovf == nullptr && catcher.caught == 2);
      std::cout << "ovf_ok=" << ovf_ok << " caught=" << catcher.caught << std::endl;
      if (!ovf_ok) ++fails;
    }

    std::cout << (fails == 0 ? "** UVM TEST PASSED **" : "!! UVM TEST FAILED !!") << std::endl;
    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  run_test("test");
  return 0;
}
