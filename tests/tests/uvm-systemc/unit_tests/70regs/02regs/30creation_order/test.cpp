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

// The registers, memories and sub-blocks of a register model are
// returned in the order in which they were created, wherever the
// objects happen to be located in memory. The classes of this test
// allocate their objects at descending addresses, the opposite of what
// the default allocator usually does. The order is kept in the model
// and does not rely on objects which may have been deleted.

#include <systemc>
#include <uvm>

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

using namespace uvm;

// Hands out memory from the end of a buffer towards its start
void* descending_alloc( std::size_t size )
{
  static const std::size_t pool_size = 1 << 20;
  static char* pool = new char[pool_size];
  static std::size_t used = 0;

  const std::size_t align = alignof(std::max_align_t);
  size = (size + align - 1) / align * align;
  used += size;

  if (used > pool_size)
    UVM_FATAL("Test", "descending_alloc: pool is too small");

  return pool + pool_size - used;
}

class my_reg : public uvm_reg
{
 public:
  uvm_reg_field* data;

  my_reg( const std::string& name = "my_reg" ) : uvm_reg(name, 32, UVM_NO_COVERAGE)
  {}

  static void* operator new( std::size_t size ) { return descending_alloc(size); }
  static void operator delete( void* ) {}

  virtual void build()
  {
    data = uvm_reg_field::type_id::create("data");
    data->configure(this, 32, 0, "RW", false, 0x0, true, false, true);
  }
};

class my_mem : public uvm_mem
{
 public:
  my_mem( const std::string& name = "my_mem" ) : uvm_mem(name, 16, 32, "RW", UVM_NO_COVERAGE)
  {}

  static void* operator new( std::size_t size ) { return descending_alloc(size); }
  static void operator delete( void* ) {}
};

class sub_blk : public uvm_reg_block
{
 public:
  sub_blk( const std::string& name = "sub_blk" ) : uvm_reg_block(name)
  {}

  static void* operator new( std::size_t size ) { return descending_alloc(size); }
  static void operator delete( void* ) {}
};

class blk : public uvm_reg_block
{
 public:
  std::vector<my_reg*> regs;
  std::vector<my_mem*> mems;
  std::vector<sub_blk*> blks;

  blk( const std::string& name = "blk" ) : uvm_reg_block(name)
  {}

  virtual void build()
  {
    default_map = create_map("default_map", 0x0, 4, UVM_LITTLE_ENDIAN);

    for (int i = 0; i < 4; i++)
    {
      my_reg* r = new my_reg("reg" + std::to_string(i));
      r->configure(this, nullptr, "");
      r->build();
      default_map->add_reg(r, 0x10 * i, "RW");
      regs.push_back(r);
    }

    for (int i = 0; i < 3; i++)
    {
      my_mem* m = new my_mem("mem" + std::to_string(i));
      m->configure(this, "");
      default_map->add_mem(m, 0x1000 * (i + 1), "RW");
      mems.push_back(m);
    }

    for (int i = 0; i < 3; i++)
    {
      sub_blk* b = new sub_blk("sub" + std::to_string(i));
      b->configure(this, "");
      blks.push_back(b);
    }

    lock_model();
  }
};

template <typename T>
void show( const std::string& what, const std::vector<T*>& objects )
{
  std::cout << what << ":";
  for (unsigned int i = 0; i < objects.size(); i++)
    std::cout << " " << objects[i]->get_name();
  std::cout << std::endl;
}

int sc_main(int, char*[])
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_report_server* svr = cs->get_report_server();

  // A register model which was deleted does not disturb the models
  // which are created after it.
  uvm_reg_block* old = new uvm_reg_block("old");
  old->create_map("default_map", 0x0, 4, UVM_LITTLE_ENDIAN);
  old->lock_model();
  delete old;

  blk* b = new blk();
  b->build();

  if (!(b->regs[0] > b->regs[1] && b->mems[0] > b->mems[1] && b->blks[0] > b->blks[1]))
    UVM_ERROR("Test", "The objects are not located at descending addresses");

  std::vector<uvm_reg*> regs;
  std::vector<uvm_mem*> mems;
  std::vector<uvm_reg_block*> blks;

  b->get_registers(regs);
  show("registers of the block", regs);

  regs.clear();
  b->default_map->get_registers(regs);
  show("registers of the map", regs);

  b->get_memories(mems);
  show("memories of the block", mems);

  mems.clear();
  b->default_map->get_memories(mems);
  show("memories of the map", mems);

  b->get_blocks(blks);
  show("blocks of the block", blks);

  b->default_map->print();

  svr->report_summarize();

  if (svr->get_severity_count(UVM_FATAL) +
      svr->get_severity_count(UVM_ERROR) == 0)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "!! UVM TEST FAILED !!" << std::endl;

  return 0;
}
