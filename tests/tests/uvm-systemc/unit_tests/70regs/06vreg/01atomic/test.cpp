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

// Accesses to the fields of a virtual register are atomic: a second
// access waits until the first one has completed. If a thread is killed
// while it holds the access semaphore, uvm_vreg::reset() puts the key
// back so that other threads can access the virtual register again.

#include <systemc>
#include <uvm>

using namespace uvm;
using namespace sc_core;

// Backdoor that stores the memory content in an array
class mem_bkdr : public uvm_reg_backdoor
{
public:
  uvm_reg_data_t data[8];

  mem_bkdr( const std::string& name = "mem_bkdr" ) : uvm_reg_backdoor(name)
  {
    for (int i = 0; i < 8; i++) data[i] = 0;
  }

  virtual void write( uvm_reg_item* rw )
  {
    data[rw->offset.to_uint64()] = rw->value[0];
    rw->status = UVM_IS_OK;
  }

  virtual void read( uvm_reg_item* rw )
  {
    rw->value[0] = data[rw->offset.to_uint64()];
    rw->status = UVM_IS_OK;
  }
};

// Virtual field whose pre_write takes 10 ns, so that the field write
// lasts long enough for a second thread to try an access
class slow_vfield : public uvm_vreg_field
{
public:
  slow_vfield( const std::string& name = "slow_vfield" ) : uvm_vreg_field(name) {}

  virtual void pre_write( unsigned long idx, uvm_reg_data_t& wdat,
                          uvm_path_e& path, uvm_reg_map*& map )
  {
    std::cout << sc_time_stamp() << ": write [" << idx << "] start" << std::endl;
    wait(10, SC_NS);
  }

  virtual void post_write( unsigned long idx, uvm_reg_data_t wdat,
                           uvm_path_e path, uvm_reg_map* map,
                           uvm_status_e& status )
  {
    std::cout << sc_time_stamp() << ": write [" << idx << "] end" << std::endl;
  }
};

class blk_t : public uvm_reg_block
{
public:
  uvm_mem* mem{nullptr};
  uvm_vreg* vr{nullptr};
  slow_vfield* f{nullptr};
  mem_bkdr* bkdr{nullptr};

  blk_t( const std::string& name = "blk" ) : uvm_reg_block(name, UVM_NO_COVERAGE) {}

  UVM_OBJECT_UTILS(blk_t);

  void build()
  {
    default_map = create_map("default_map", 0, 4, UVM_LITTLE_ENDIAN);

    mem = new uvm_mem("mem", 8, 32, "RW", UVM_NO_COVERAGE);
    mem->configure(this);
    default_map->add_mem(mem, 0x0, "RW");

    bkdr = new mem_bkdr();
    mem->set_backdoor(bkdr);

    vr = new uvm_vreg("vr", 32);
    vr->configure(this, mem, 4, 0, 1);

    f = new slow_vfield("f");
    f->configure(vr, 32, 0);

    lock_model();
  }
};

class test : public uvm_test
{
public:
  blk_t* blk{nullptr};

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    blk = new blk_t("blk");
    blk->build();
  }

  void do_write( unsigned long idx, unsigned int value )
  {
    uvm_status_e status;
    blk->f->write(idx, status, value, UVM_BACKDOOR);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // Two concurrent writes are serialized
    sc_process_handle a = sc_spawn(sc_bind(&test::do_write, this, 0, 0x11));
    sc_process_handle b = sc_spawn(sc_bind(&test::do_write, this, 1, 0x22));
    wait(a.terminated_event() & b.terminated_event());

    // A thread killed during a write keeps the semaphore ...
    wait(10, SC_NS);
    sc_process_handle c = sc_spawn(sc_bind(&test::do_write, this, 2, 0x33));
    wait(5, SC_NS);
    c.kill();
    std::cout << sc_time_stamp() << ": write [2] killed" << std::endl;

    sc_process_handle d = sc_spawn(sc_bind(&test::do_write, this, 3, 0x44));
    wait(20, SC_NS);
    std::cout << sc_time_stamp() << ": write [3] "
              << (d.terminated() ? "done" : "waiting") << std::endl;

    // ... until reset() puts it back
    blk->vr->reset();
    std::cout << sc_time_stamp() << ": reset" << std::endl;
    if (!d.terminated())
      wait(d.terminated_event());

    for (int i = 0; i < 4; i++)
      std::cout << "mem[" << i << "] = 0x" << std::hex
                << blk->bkdr->data[i].to_uint64() << std::dec << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
