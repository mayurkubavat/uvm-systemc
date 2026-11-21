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

// uvm_reg::include_coverage stores the coverage models to include in the
// resource database, in the "uvm_reg::" scope. build_coverage of
// registers, memories and blocks returns the models requested by the
// abstraction class that are also included for its full name.

#include <systemc>
#include <uvm>

using namespace uvm;

class cov_reg : public uvm_reg
{
public:
  uvm_reg_field* f{nullptr};

  cov_reg( const std::string& name = "cov_reg" )
  : uvm_reg(name, 8, UVM_NO_COVERAGE)
  {
    // what a register model generator does in the constructor
    add_coverage(build_coverage(UVM_CVR_REG_BITS | UVM_CVR_FIELD_VALS));
  }

  UVM_OBJECT_UTILS(cov_reg);

  virtual void build()
  {
    f = uvm_reg_field::type_id::create("f");
    f->configure(this, 8, 0, "RW", false, 0, true, false, true);
  }

  uvm_reg_cvr_t built( uvm_reg_cvr_t models ) { return build_coverage(models); }
};

class cov_mem : public uvm_mem
{
public:
  cov_mem( const std::string& name = "cov_mem" )
  : uvm_mem(name, 16, 8, "RW", UVM_NO_COVERAGE)
  {
    add_coverage(build_coverage(UVM_CVR_ALL));
  }

  uvm_reg_cvr_t built( uvm_reg_cvr_t models ) { return build_coverage(models); }
};

class cov_blk : public uvm_reg_block
{
public:
  cov_reg* r1{nullptr};
  cov_reg* r2{nullptr};
  cov_mem* m{nullptr};

  cov_blk( const std::string& name = "blk" )
  : uvm_reg_block(name, UVM_NO_COVERAGE)
  {}

  UVM_OBJECT_UTILS(cov_blk);

  void build()
  {
    add_coverage(build_coverage(UVM_CVR_ALL));

    default_map = create_map("default_map", 0, 1, UVM_LITTLE_ENDIAN);

    r1 = new cov_reg("r1");
    r1->configure(this);
    r1->build();
    default_map->add_reg(r1, 0x0);

    r2 = new cov_reg("r2");
    r2->configure(this);
    r2->build();
    default_map->add_reg(r2, 0x1);

    m = new cov_mem("m");
    m->configure(this);
    default_map->add_mem(m, 0x10);

    lock_model();
  }

  uvm_reg_cvr_t built( uvm_reg_cvr_t models ) { return build_coverage(models); }
};

static std::string models( uvm_reg_cvr_t m )
{
  std::ostringstream s;
  s << "0x" << std::hex << m;
  return s.str();
}

class test : public uvm_test
{
public:
  cov_blk* blk{nullptr};

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // The first match in the resource database wins
    uvm_reg::include_coverage("r2", UVM_CVR_REG_BITS);
    uvm_reg::include_coverage("blk", UVM_CVR_ADDR_MAP);
    uvm_reg::include_coverage("blk.*", UVM_CVR_ADDR_MAP | UVM_CVR_REG_BITS);
    uvm_reg::include_coverage("*", UVM_CVR_FIELD_VALS);

    blk = new cov_blk("blk");
    blk->build();
  }

  virtual void run_phase( uvm_phase& phase )
  {
    // models built in the constructor, where the full name has no parent
    std::cout << "r1 has REG_BITS="   << blk->r1->has_coverage(UVM_CVR_REG_BITS)
              << " FIELD_VALS="       << blk->r1->has_coverage(UVM_CVR_FIELD_VALS)
              << std::endl;
    std::cout << "r2 has REG_BITS="   << blk->r2->has_coverage(UVM_CVR_REG_BITS)
              << " FIELD_VALS="       << blk->r2->has_coverage(UVM_CVR_FIELD_VALS)
              << std::endl;
    std::cout << "m has "  << models(blk->m->built(UVM_CVR_ALL)) << std::endl;
    std::cout << "blk has ADDR_MAP=" << blk->has_coverage(UVM_CVR_ADDR_MAP)
              << " REG_BITS="        << blk->has_coverage(UVM_CVR_REG_BITS)
              << std::endl;

    // models for the full name in the block
    std::cout << "blk.r1 build_coverage(ALL) = " << models(blk->r1->built(UVM_CVR_ALL)) << std::endl;
    std::cout << "blk.r1 build_coverage(FIELD_VALS) = " << models(blk->r1->built(UVM_CVR_FIELD_VALS)) << std::endl;

    // set_coverage turns on only the models that are present
    std::cout << "r2 set_coverage(ALL) = " << models(blk->r2->set_coverage(UVM_CVR_ALL)) << std::endl;
    std::cout << "r2 get_coverage(REG_BITS)=" << blk->r2->get_coverage(UVM_CVR_REG_BITS)
              << " get_coverage(FIELD_VALS)=" << blk->r2->get_coverage(UVM_CVR_FIELD_VALS)
              << std::endl;
    std::cout << "r2 set_coverage(NO_COVERAGE) = " << models(blk->r2->set_coverage(UVM_NO_COVERAGE)) << std::endl;
    std::cout << "r2 get_coverage(REG_BITS)=" << blk->r2->get_coverage(UVM_CVR_REG_BITS) << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
