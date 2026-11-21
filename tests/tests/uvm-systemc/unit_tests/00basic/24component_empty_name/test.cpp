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

// Test of the name of a component created with an empty name: the leaf
// name is the name SystemC generates for the module, so that get_name,
// get_full_name and the child lookup of the parent agree, and two such
// children do not collide. Other names are kept as given: a duplicate
// name is rejected by the parent, and a name with an inner space is
// found by lookup.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

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

  uvm_component* c[4];

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    c[0] = leaf::type_id::create("", this);
    c[1] = leaf::type_id::create("", this);
    c[2] = leaf::type_id::create("named", this);
    c[3] = leaf::type_id::create("a b", this);
    leaf::type_id::create("named", this); // duplicate
  }

  virtual void end_of_elaboration_phase( uvm_phase& phase )
  {
    for (int i = 0; i < 4; i++)
      std::cout << "get_name '" << c[i]->get_name() << "' get_full_name '"
                << c[i]->get_full_name() << "' lookup "
                << (lookup(c[i]->get_name()) == c[i] ? "ok" : "failed") << std::endl;
    std::cout << "children: " << get_num_children() << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
