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

// Test of the randomization of a register model: uvm_reg_field::randomize,
// uvm_reg::randomize and uvm_reg_block::randomize.

#include <systemc>
#include <uvm>

#include <set>

using namespace uvm;

#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::cout << "FAILED: " << #cond << std::endl; \
      pass = false; \
    } \
  } while (false)

class reg_ctrl : public uvm_reg
{
 public:
  uvm_reg_field* mode;    // 3 bits, RW, random
  uvm_reg_field* status;  // 5 bits, RO: never random
  uvm_reg_field* fixed;   // 8 bits, RW, not random
  uvm_reg_field* data;    // 16 bits, RW, random

  int num_pre;
  int num_post;

  reg_ctrl( const std::string& name = "reg_ctrl" )
    : uvm_reg(name, 32, UVM_NO_COVERAGE), num_pre(0), num_post(0)
  {}

  virtual void build()
  {
    mode   = uvm_reg_field::type_id::create("mode");
    status = uvm_reg_field::type_id::create("status");
    fixed  = uvm_reg_field::type_id::create("fixed");
    data   = uvm_reg_field::type_id::create("data");

    //                size lsb access volatile reset has_reset is_rand indiv
    mode->configure(  this,  3,  0, "RW", false, 0x0,  true, true,  false);
    status->configure(this,  5,  3, "RO", false, 0x15, true, true,  false);
    fixed->configure( this,  8,  8, "RW", false, 0x5a, true, false, false);
    data->configure(  this, 16, 16, "RW", false, 0x0,  true, true,  false);
  }

  virtual void pre_randomize()
  {
    num_pre++;
  }

  // constraint: mode 7 is reserved
  virtual void post_randomize()
  {
    num_post++;
    if (mode->get() == 7)
      mode->set(0);
  }
};

class reg_wide : public uvm_reg
{
 public:
  uvm_reg_field* value; // the full width of uvm_reg_data_t

  reg_wide( const std::string& name = "reg_wide" )
    : uvm_reg(name, UVM_REG_DATA_WIDTH, UVM_NO_COVERAGE)
  {}

  virtual void build()
  {
    value = uvm_reg_field::type_id::create("value");
    value->configure(this, UVM_REG_DATA_WIDTH, 0, "RW", false, 0x0, true, true, false);
  }
};

class block : public uvm_reg_block
{
 public:
  reg_ctrl* ctrl;
  reg_wide* wide;

  int num_post;

  block( const std::string& name = "block" )
    : uvm_reg_block(name, UVM_NO_COVERAGE), ctrl(nullptr), wide(nullptr), num_post(0)
  {}

  virtual void build()
  {
    ctrl = new reg_ctrl("ctrl");
    ctrl->configure(this);
    ctrl->build();

    wide = new reg_wide("wide");
    wide->configure(this);
    wide->build();

    default_map = create_map("default_map", 0x0, UVM_REG_DATA_WIDTH / 8, UVM_LITTLE_ENDIAN);
    default_map->add_reg(ctrl, 0x00, "RW");
    default_map->add_reg(wide, 0x10, "RW");

    lock_model();
  }

  virtual void post_randomize()
  {
    num_post++;
  }
};

class top_block : public uvm_reg_block
{
 public:
  block* sub;

  top_block( const std::string& name = "top_block" )
    : uvm_reg_block(name, UVM_NO_COVERAGE), sub(nullptr)
  {}

  virtual void build()
  {
    sub = new block("sub");
    sub->configure(this);
    sub->build();

    default_map = create_map("default_map", 0x0, UVM_REG_DATA_WIDTH / 8, UVM_LITTLE_ENDIAN);
    default_map->add_submap(sub->default_map, 0x100);

    lock_model();
  }
};

// The values of the field data of a register in 20 randomizations
std::vector<uvm_reg_data_t> data_values( reg_ctrl* rg )
{
  std::vector<uvm_reg_data_t> values;

  for (int i = 0; i < 20; i++)
  {
    rg->randomize();
    values.push_back(rg->data->get());
  }
  return values;
}

// a field in a register of its own, named "fields"
std::vector<uvm_reg_data_t> field_values( const std::string& name )
{
  uvm_reg_field* f = uvm_reg_field::type_id::create(name);
  f->configure(new reg_wide("fields"), 32, 0, "RW", false, 0, true, true, false);
  std::vector<uvm_reg_data_t> values;
  for (int i = 0; i < 5; i++)
  {
    f->randomize();
    values.push_back(f->get());
  }
  return values;
}

reg_ctrl* new_reg( const std::string& name )
{
  reg_ctrl* rg = new reg_ctrl(name);
  rg->build();
  rg->reset();
  return rg;
}

int sc_main(int, char*[])
{
  bool pass = true;

  top_block* top = new top_block("top");
  top->build();
  top->reset();

  reg_ctrl* ctrl = top->sub->ctrl;
  reg_wide* wide = top->sub->wide;

  std::set<sc_dt::uint64> modes, datas, wides;
  bool wide_msb = false;

  // randomization of the top block reaches the registers of the sub-block
  for (int i = 0; i < 200; i++)
  {
    CHECK(top->randomize());

    // random fields get values which fit their size
    CHECK(ctrl->mode->get() < 8);
    CHECK(ctrl->data->get() < 0x10000);

    // the constraint in post_randomize holds
    CHECK(ctrl->mode->get() != 7);

    // read-only fields and fields which are not random keep their value
    CHECK(ctrl->status->get() == 0x15);
    CHECK(ctrl->fixed->get() == 0x5a);

    // the value of the register is made of the values of its fields
    CHECK(ctrl->get() == ((ctrl->data->get() << 16) | (0x5a << 8) | (0x15 << 3) | ctrl->mode->get()));

    modes.insert(ctrl->mode->get().to_uint64());
    datas.insert(ctrl->data->get().to_uint64());
    wides.insert(wide->value->get().to_uint64());
    if (wide->value->get()[UVM_REG_DATA_WIDTH - 1] == 1)
      wide_msb = true;
  }

  // the values are spread over the range of the fields
  CHECK(modes.size() == 7);
  CHECK(datas.size() > 190);
  CHECK(wides.size() == 200);
  CHECK(wide_msb);

  // the hooks are called once per randomization
  CHECK(ctrl->num_pre == 200);
  CHECK(ctrl->num_post == 200);
  CHECK(top->sub->num_post == 200);

  // the random values are desired values: the register needs an update
  CHECK(ctrl->needs_update());

  // a register and a field can be randomized on their own
  uvm_reg_data_t fixed_before = wide->value->get();
  CHECK(ctrl->randomize());
  CHECK(wide->value->get() == fixed_before);
  CHECK(ctrl->num_post == 201);
  CHECK(ctrl->data->randomize());
  CHECK(ctrl->num_post == 201);

  // rand mode: read-only fields are never random, others can be switched
  CHECK(ctrl->mode->get_rand_mode());
  CHECK(!ctrl->status->get_rand_mode());
  CHECK(!ctrl->fixed->get_rand_mode());

  ctrl->data->set_rand_mode(false);
  ctrl->data->set(0x1234);
  for (int i = 0; i < 20; i++)
    ctrl->randomize();
  CHECK(ctrl->data->get() == 0x1234);

  ctrl->fixed->set_rand_mode(true);
  std::set<sc_dt::uint64> fixeds;
  for (int i = 0; i < 20; i++)
  {
    ctrl->randomize();
    fixeds.insert(ctrl->fixed->get().to_uint64());
  }
  CHECK(fixeds.size() > 10);

  // a read-only field is never random, however the policy is written
  uvm_reg_field* lower = uvm_reg_field::type_id::create("lower");
  reg_wide* holder = new reg_wide("holder");
  lower->configure(holder, 4, 0, "ro", false, 0x9, true, true, false);
  CHECK(!lower->get_rand_mode());

  // The values of a field depend on the global seed and on its name:
  // the same seed gives the same values in the same register after the
  // seed has been set again, and another seed gives other values.
  uvm_set_random_seed(11);
  reg_ctrl* rg_a = new_reg("rg");
  std::vector<uvm_reg_data_t> v1 = data_values(rg_a);
  uvm_set_random_seed(12);
  std::vector<uvm_reg_data_t> v3 = data_values(rg_a);
  uvm_set_random_seed(11);
  std::vector<uvm_reg_data_t> v4 = data_values(rg_a);
  CHECK(v1 == v4);
  CHECK(v1 != v3);

  // Registers with the same name, as in two models which have the same
  // name, get different values, and so do registers with other names.
  reg_ctrl* rg_b = new_reg("rg");
  reg_ctrl* rg_c = new_reg("rg");
  reg_ctrl* rg_y = new_reg("other");
  std::vector<uvm_reg_data_t> vb = data_values(rg_b);
  std::vector<uvm_reg_data_t> vc = data_values(rg_c);
  std::vector<uvm_reg_data_t> vy = data_values(rg_y);
  CHECK(vb != v1);
  CHECK(vc != v1);
  CHECK(vc != vb);
  CHECK(vy != v1);

  // A register keeps its own values after it has been randomized with
  // another seed and the seed has been set back.
  uvm_set_random_seed(12);
  CHECK(data_values(rg_b) != vb);
  data_values(rg_a);
  uvm_set_random_seed(11);
  CHECK(data_values(rg_b) == vb);
  CHECK(data_values(rg_a) == v1);

  // The values do not depend on the order in which registers with
  // different names are randomized, also after they have been reseeded.
  uvm_set_random_seed(13);
  reg_ctrl* rg_p = new_reg("p");
  reg_ctrl* rg_q = new_reg("q");
  std::vector<uvm_reg_data_t> vp = data_values(rg_p);
  std::vector<uvm_reg_data_t> vq = data_values(rg_q);
  uvm_set_random_seed(14);
  data_values(rg_q);
  data_values(rg_p);
  uvm_set_random_seed(13);
  CHECK(data_values(rg_q) == vq);
  CHECK(data_values(rg_p) == vp);

  // A field whose name is that of another field plus one character
  // does not get the values of a later field with the shorter name.
  uvm_set_random_seed(15);
  std::vector<uvm_reg_data_t> vlong = field_values("g1");
  bool repeated = false;
  for (int i = 0; i < 60; i++)
    if (field_values("g") == vlong)
      repeated = true;
  CHECK(!repeated);

  if (pass)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "** UVM TEST FAILED **" << std::endl;

  return 0;
}
