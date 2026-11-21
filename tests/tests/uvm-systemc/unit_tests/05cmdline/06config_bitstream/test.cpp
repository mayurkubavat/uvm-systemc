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

// Test of the command line argument +uvm_set_config_bitstream. The
// value is stored for the type uvm_bitstream_t, as with
// uvm_config_db<uvm_bitstream_t>::set, and not for the type int.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  void show( const std::string& field )
  {
    uvm_bitstream_t bits = 0;
    int number = 0;

    if (uvm_config_db<uvm_bitstream_t>::get(this, "", field, bits))
      std::cout << field << " as uvm_bitstream_t = 0x" << std::hex
                << bits.to_uint64() << std::dec << std::endl;
    else
      std::cout << field << " not found as uvm_bitstream_t" << std::endl;

    if (uvm_config_db<int>::get(this, "", field, number))
      std::cout << field << " as int = " << number << std::endl;
    else
      std::cout << field << " not found as int" << std::endl;
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    show("mask");
    show("wide");
    show("count");
    show("flags");
    show("all_ones");
    show("number");
    show("bad");
  }

  virtual void report_phase( uvm_phase& phase )
  {
    std::cout << "** UVM TEST PASSED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}

int main(int, char*[])
{
  const char* argv[] = {
    "test",
    "+uvm_set_config_bitstream=test,mask,'hFFFF_FFFF",
    "+uvm_set_config_bitstream=test,wide,0x123456789abcdef0",
    "+uvm_set_config_bitstream=*,count,12",
    "+UVM_SET_CONFIG_BITSTREAM=test,flags,'b1010",
    "+uvm_set_config_bitstream=test,all_ones,18446744073709551615",
    // underscores after the first digit are ignored, for both arguments
    "+uvm_set_config_int=test,number,1_000",
    // not well formed
    "+uvm_set_config_bitstream=test,bad,_1",
    "+uvm_set_config_int=test,bad,-_1",
    "+uvm_set_config_int=test,bad,18446744073709551615",
    "+uvm_set_config_bitstream=test,bad,many",
    "+uvm_set_config_bitstream=test,bad",
    "+uvm_set_config_bitstream=test,bad,1,2"
  };
  int argc = sizeof(argv) / sizeof(argv[0]);

  return sc_core::sc_elab_and_sim(argc, const_cast<char**>(argv));
}
