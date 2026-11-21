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

// Test of uvm_coreservice_t::set_component_visitor and
// get_component_visitor: the component visitor is applied top-down to
// the entire hierarchy at the end of elaboration, instead of the default
// uvm_component_name_check_visitor.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class count_visitor : public uvm_visitor<uvm_component>
{
 public:
  count_visitor() : uvm_visitor<uvm_component>("count_visitor"), count(0) {}

  int count;

  virtual void begin_v() { std::cout << "begin_v" << std::endl; }
  virtual void end_v() { std::cout << "end_v: " << count << " components" << std::endl; }

  virtual void visit( uvm_component* node )
  {
    count++;
    std::cout << "visit '" << node->get_name() << "' (" << node->get_type_name()
              << ")" << std::endl;
  }
};

class leaf : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(leaf);
  leaf( uvm_component_name name ) : uvm_component(name) {}
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    leaf::type_id::create("ok", this);
    leaf::type_id::create("bad+name", this); // not checked by count_visitor
  }

  virtual void end_of_elaboration_phase( uvm_phase& phase )
  {
    std::cout << "test::end_of_elaboration_phase" << std::endl;
  }
};

int sc_main(int, char*[])
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();

  uvm_visitor<uvm_component>* dflt = cs->get_component_visitor();
  std::cout << "default visitor: " << dflt->get_name() << " ("
            << (dynamic_cast<uvm_component_name_check_visitor*>(dflt) != nullptr
                ? "uvm_component_name_check_visitor" : "other")
            << ")" << std::endl;

  count_visitor v;
  cs->set_component_visitor(&v);
  std::cout << "visitor replaced: "
            << (cs->get_component_visitor() == &v ? "yes" : "no") << std::endl;

  run_test("test");
  return 0;
}
