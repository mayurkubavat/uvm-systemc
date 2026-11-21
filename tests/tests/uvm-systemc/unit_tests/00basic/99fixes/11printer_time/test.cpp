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

// Regression test for uvm_printer::print_time(). A time field must be
// printed as a time (type "time", value with its unit), with all three
// standard printers.

#include <systemc>
#include <uvm>

#include <iostream>

using namespace uvm;

class timed_obj : public uvm::uvm_object
{
 public:
  UVM_OBJECT_UTILS(timed_obj);

  sc_core::sc_time t_zero;
  sc_core::sc_time t_ns;
  sc_core::sc_time t_us;

  timed_obj( const std::string& name = "timed_obj" )
  : uvm::uvm_object(name),
    t_zero(sc_core::SC_ZERO_TIME),
    t_ns(10, sc_core::SC_NS),
    t_us(2500, sc_core::SC_NS)
  {}

  virtual void do_print( const uvm::uvm_printer& printer ) const
  {
    printer.print_time("t_zero", t_zero);
    printer.print_time("t_ns", t_ns);
    printer.print_time("t_us", t_us);
  }
};

int sc_main(int, char*[])
{
  timed_obj obj("obj");

  uvm_table_printer table;
  uvm_tree_printer tree;
  uvm_line_printer line;

  obj.print(&table);
  obj.print(&tree);
  obj.print(&line);

  std::cout << "** UVM TEST PASSED **" << std::endl;
  return 0;
}
