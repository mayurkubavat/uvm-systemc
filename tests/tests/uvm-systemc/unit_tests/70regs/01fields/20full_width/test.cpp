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

// A field of 64 bits takes every 64 bit value without a warning, and a
// narrower field still warns about values which are too wide.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class my_reg : public uvm_reg
{
 public:
  uvm_reg_field* full;

  my_reg( const std::string& name = "my_reg", unsigned int n_bits = 64 )
  : uvm_reg(name, n_bits, UVM_NO_COVERAGE), full(nullptr)
  {}

  void build( unsigned int n_bits )
  {
    full = uvm_reg_field::type_id::create("full");
    full->configure(this, n_bits, 0, "RW", false, 0x0, true, false, true);
  }
};

class blk : public uvm_reg_block
{
 public:
  my_reg* wide;
  my_reg* narrow;

  blk( const std::string& name = "blk" ) : uvm_reg_block(name)
  {}

  virtual void build()
  {
    default_map = create_map("default_map", 0x0, 8, UVM_LITTLE_ENDIAN);

    wide = new my_reg("wide", 64);
    wide->configure(this, nullptr, "");
    wide->build(64);
    default_map->add_reg(wide, 0x0, "RW");

    narrow = new my_reg("narrow", 8);
    narrow->configure(this, nullptr, "");
    narrow->build(8);
    default_map->add_reg(narrow, 0x8, "RW");

    lock_model();
  }
};

int sc_main(int, char*[])
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_report_server* svr = cs->get_report_server();

  blk* b = new blk();
  b->build();

  const uvm_reg_data_t big = 0xfedcba9876543210ULL;

  b->wide->full->set(big);
  std::cout << "wide field set to 0x" << std::hex
            << b->wide->full->get().to_uint64() << std::dec << std::endl;

  b->wide->full->set(1);
  std::cout << "wide field set to 0x" << std::hex
            << b->wide->full->get().to_uint64() << std::dec << std::endl;

  std::cout << "a warning for the narrow field is expected" << std::endl;
  b->narrow->full->set(0x1ff);
  std::cout << "narrow field set to 0x" << std::hex
            << b->narrow->full->get().to_uint64() << std::dec << std::endl;

  b->narrow->full->set(0xff);
  std::cout << "narrow field set to 0x" << std::hex
            << b->narrow->full->get().to_uint64() << std::dec << std::endl;

  svr->report_summarize();

  if (svr->get_severity_count(UVM_WARNING) == 1 &&
      svr->get_severity_count(UVM_ERROR) + svr->get_severity_count(UVM_FATAL) == 0)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "!! UVM TEST FAILED !!" << std::endl;

  return 0;
}
