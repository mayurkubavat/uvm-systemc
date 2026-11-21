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

// Radix formatting of the printer and the comparer, as in UVM-SV:
// - the comparer shows 'b%0b, 'o%0o, %0d and 'h%0x, with the same
//   digits as the printer
// - UVM_UNSIGNED values are never negative, and wide values in
//   UVM_HEX and UVM_OCT have no leading zeros
// - a UVM_TIME value wider than 64 bits saturates
// - printing a UVM_TIME value before the simulation runs does not fix
//   the time resolution, so sc_set_time_resolution can still be called

#include <systemc>
#include <uvm>

using namespace uvm;

class item : public uvm::uvm_object
{
public:
  item( const std::string& name = "item" ) : uvm::uvm_object(name) {}

  UVM_OBJECT_UTILS(item);

  virtual void do_print( const uvm::uvm_printer& printer ) const
  {
    uvm_bitstream_t msb = 1;
    msb <<= 99;
    uvm_bitstream_t bit70 = 1;
    bit70 <<= 70;

    printer.print_field_int("uns_all64", ~(sc_dt::uint64)0, 64, UVM_UNSIGNED);
    printer.print_field("wide_uns", msb, 100, UVM_UNSIGNED);
    printer.print_field("wide_hex", uvm_bitstream_t(0x41), 100, UVM_HEX);
    printer.print_field("wide_oct", uvm_bitstream_t(0x41), 100, UVM_OCT);
    printer.print_field_int("t", 5, 64, UVM_TIME);
    printer.print_field("wide_t", bit70, 100, UVM_TIME);
  }
};

class test : public uvm_test
{
public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    item a("a");
    a.print();

    uvm::uvm_comparer* c = uvm::uvm_comparer::init();
    c->show_max = 20;
    c->sev = UVM_INFO;
    c->compare_field_int("bin_8", 1, 2, 8, UVM_BIN);
    c->compare_field_int("oct_8", 9, 10, 8, UVM_OCT);
    c->compare_field_int("hex_8", 9, 10, 8, UVM_HEX);
    c->compare_field_int("dec_8", 9, 10, 8, UVM_DEC);
    c->compare_field_int("enum_8", 9, 10, 8, UVM_ENUM);
    c->compare_field("bin_70", uvm_bitstream_t(1), uvm_bitstream_t(2), 70, UVM_BIN);
    c->compare_field("oct_70", uvm_bitstream_t(9), uvm_bitstream_t(10), 70, UVM_OCT);
    c->compare_field("hex_70", uvm_bitstream_t(9), uvm_bitstream_t(10), 70, UVM_HEX);
    c->compare_field("dec_70", uvm_bitstream_t(9), uvm_bitstream_t(10), 70, UVM_DEC);
  }
};

int sc_main(int, char*[])
{
  // print a time field during elaboration, then set the time resolution
  item early("early");
  early.print();
  sc_core::sc_set_time_resolution(1, sc_core::SC_NS);

  run_test("test");
  return 0;
}
