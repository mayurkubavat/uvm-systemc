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

// The address interval between the locations of a memory is the same
// for a memory with one location, for a memory with several locations,
// and after a memory has been moved to another offset.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class my_mem : public uvm_mem
{
 public:
  my_mem( const std::string& name, unsigned long size )
  : uvm_mem(name, size, 32, "RW", UVM_NO_COVERAGE)
  {}
};

class blk : public uvm_reg_block
{
 public:
  my_mem* one;
  my_mem* eight;

  blk( const std::string& name = "blk" ) : uvm_reg_block(name)
  {}

  virtual void build()
  {
    default_map = create_map("default_map", 0x0, 4, UVM_LITTLE_ENDIAN);

    one = new my_mem("one", 1);
    one->configure(this, "");
    default_map->add_mem(one, 0x100, "RW");

    eight = new my_mem("eight", 8);
    eight->configure(this, "");
    default_map->add_mem(eight, 0x200, "RW");

    lock_model();
  }
};

void show( blk* b, uvm_mem* m )
{
  uvm_reg_map_info* info = b->default_map->get_mem_map_info(m);
  std::cout << m->get_name() << ": offset 0x" << std::hex << info->offset.to_uint64()
            << ", addresses 0x" << info->mem_range.min.to_uint64()
            << " to 0x" << info->mem_range.max.to_uint64()
            << ", stride " << std::dec << info->mem_range.stride << std::endl;
}

int sc_main(int, char*[])
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_report_server* svr = cs->get_report_server();

  blk* b = new blk();
  b->build();

  show(b, b->one);
  show(b, b->eight);

  b->eight->set_offset(b->default_map, 0x400);
  show(b, b->eight);

  b->one->set_offset(b->default_map, 0x300);
  show(b, b->one);

  svr->report_summarize();

  if (svr->get_severity_count(UVM_ERROR) + svr->get_severity_count(UVM_FATAL) == 0)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "!! UVM TEST FAILED !!" << std::endl;

  return 0;
}
