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

// Reproducing test for Fix 6 (uvm_reg_block::convert2string):
//   Verifies uvm_reg_block::convert2string() outputs block name, maps,
//   mapped & unmapped registers, memories, virtual registers, and
//   recursive sub-blocks without duplicating multi-map registers.

#include <iostream>
#include <string>

#include <systemc>
#include <uvm>

using namespace uvm;

class simple_reg : public uvm_reg {
 public:
  UVM_OBJECT_UTILS(simple_reg);

  uvm_reg_field* f = nullptr;

  explicit simple_reg(const std::string& name = "simple_reg")
      : uvm_reg(name, 32, UVM_NO_COVERAGE) {}

  virtual void build() {
    f = uvm_reg_field::type_id::create("f");
    f->configure(this, 8, 0, "RW", false, 0x0, true, false, true);
  }
};

class child_blk : public uvm_reg_block {
 public:
  UVM_OBJECT_UTILS(child_blk);

  simple_reg* sub_reg = nullptr;

  explicit child_blk(const std::string& name = "child_blk")
      : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  virtual void build() {
    default_map = create_map("sub_map", 0x0, 4, UVM_LITTLE_ENDIAN);
    sub_reg = simple_reg::type_id::create("sub_reg");
    sub_reg->configure(this);
    sub_reg->build();
    default_map->add_reg(sub_reg, 0x0, "RW");
  }
};

class top_blk : public uvm_reg_block {
 public:
  UVM_OBJECT_UTILS(top_blk);

  uvm_reg_map* map_a = nullptr;
  uvm_reg_map* map_b = nullptr;
  simple_reg* r_mapped = nullptr;
  simple_reg* r_unmapped = nullptr;
  uvm_mem* mem = nullptr;
  uvm_vreg* vreg = nullptr;
  child_blk* sub_blk = nullptr;

  explicit top_blk(const std::string& name = "top_blk")
      : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  virtual void build() {
    map_a = create_map("map_a", 0x0, 4, UVM_LITTLE_ENDIAN);
    map_b = create_map("map_b", 0x1000, 4, UVM_BIG_ENDIAN);
    default_map = map_a;

    r_mapped = simple_reg::type_id::create("r_mapped");
    r_mapped->configure(this);
    r_mapped->build();
    map_a->add_reg(r_mapped, 0x0, "RW");
    map_b->add_reg(r_mapped, 0x0, "RW");

    r_unmapped = simple_reg::type_id::create("r_unmapped");
    r_unmapped->configure(this);
    r_unmapped->build();

    mem = new uvm_mem("mem", 16, 32, "RW", UVM_NO_COVERAGE);
    mem->configure(this);
    map_a->add_mem(mem, 0x100, "RW");

    vreg = new uvm_vreg("vreg", 32);
    vreg->configure(this, mem, 4, 0, 1);

    sub_blk = child_blk::type_id::create("sub_blk");
    sub_blk->configure(this);
    sub_blk->build();
    map_a->add_submap(sub_blk->default_map, 0x200);

    lock_model();
  }
};

static int count_substr(const std::string& haystack, const std::string& needle) {
  int cnt = 0;
  std::string::size_type pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos) {
    ++cnt;
    pos += needle.size();
  }
  return cnt;
}

class test : public uvm_test {
 public:
  UVM_COMPONENT_UTILS(test);

  explicit test(uvm_component_name name) : uvm_test(name) {}

  virtual void run_phase(uvm_phase& phase) {
    phase.raise_objection(this);

    auto* blk = top_blk::type_id::create("top_blk");
    blk->build();

    // Call polymorphically via uvm_object* so this compiles even when
    // uvm_reg_block::convert2string() is still private before the fix.
    const uvm_object* obj = blk;
    std::string img = obj->convert2string();

    bool ok = !img.empty() &&
              img.find("Block top_blk") != std::string::npos &&
              img.find("map_a") != std::string::npos &&
              img.find("map_b") != std::string::npos &&
              count_substr(img, "Register top_blk.r_mapped") == 1 &&
              img.find("Register top_blk.r_unmapped") != std::string::npos &&
              img.find("Mapped in 'top_blk.map_a' -- buswidth 4 bytes") != std::string::npos &&
              img.find("top_blk.mem") != std::string::npos &&
              img.find("vreg[4]") != std::string::npos &&
              img.find("Block top_blk.sub_blk") != std::string::npos &&
              img.find("Register top_blk.sub_blk.sub_reg") != std::string::npos;

    std::cout << "non_empty=" << (!img.empty())
              << " all_parts_ok=" << ok << std::endl;
    if (!ok) {
      std::cout << "ACTUAL_IMAGE:\n" << img << std::endl;
    }

    std::cout << (ok ? "** UVM TEST PASSED **" : "!! UVM TEST FAILED !!") << std::endl;
    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  run_test("test");
  return 0;
}
