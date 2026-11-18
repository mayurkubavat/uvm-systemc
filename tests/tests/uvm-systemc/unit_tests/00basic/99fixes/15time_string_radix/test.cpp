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

// Integral values printed or compared with the UVM_TIME radix are shown
// as a time (the value is a number of time resolution units, as %t in
// UVM-SV), and with the UVM_STRING radix as a string of 8-bit
// characters (as %s in UVM-SV).

#include <systemc>
#include <uvm>

using namespace uvm;

// "ABC" and "ABD" as 8-bit characters
static const sc_dt::uint64 ABC = 0x414243;
static const sc_dt::uint64 ABD = 0x414244;

class item : public uvm::uvm_object
{
public:
  sc_dt::uint64 t{0};
  sc_dt::uint64 s{0};

  item( const std::string& name = "item" ) : uvm::uvm_object(name) {}

  UVM_OBJECT_UTILS(item);

  virtual void do_print( const uvm::uvm_printer& printer ) const
  {
    printer.print_field_int("t", t, 64, UVM_TIME);
    printer.print_field_int("s", s, 32, UVM_STRING);
    printer.print_field("wide_t", uvm_bitstream_t(t), 100, UVM_TIME);
    printer.print_field("wide_s", uvm_bitstream_t(s), 100, UVM_STRING);
  }

  virtual bool do_compare( const uvm::uvm_object& rhs, const uvm::uvm_comparer* comparer ) const
  {
    const item* rhs_ = dynamic_cast<const item*>(&rhs);
    bool ok = true;
    ok &= comparer->compare_field_int("t", t, rhs_->t, 64, UVM_TIME);
    ok &= comparer->compare_field_int("s", s, rhs_->s, 32, UVM_STRING);
    ok &= comparer->compare_field("wide_t", uvm_bitstream_t(t), uvm_bitstream_t(rhs_->t), 100, UVM_TIME);
    ok &= comparer->compare_field("wide_s", uvm_bitstream_t(s), uvm_bitstream_t(rhs_->s), 100, UVM_STRING);
    return ok;
  }
};

class test : public uvm_test
{
public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    item a("a"), b("b");
    a.t = sc_core::sc_time(10, sc_core::SC_NS).value();
    a.s = ABC;
    b.t = sc_core::sc_time(25, sc_core::SC_US).value();
    b.s = ABD;

    a.print();

    uvm::uvm_comparer* comparer = uvm::uvm_comparer::init();
    comparer->show_max = 10;
    comparer->sev = UVM_INFO;
    bool result = a.compare(b, comparer);
    std::cout << "compare = " << result << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
