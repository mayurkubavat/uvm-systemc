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

// Test of the component traversal classes: uvm_visitor, the top-down,
// bottom-up and by-level visitor adapters, uvm_component_proxy, a user
// defined uvm_structure_proxy and uvm_component_name_check_visitor.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>
#include <vector>

using namespace uvm;

// prints each visited component and counts the begin_v and end_v calls
class print_visitor : public uvm_visitor<uvm_component>
{
 public:
  print_visitor( const std::string& name ) : uvm_visitor<uvm_component>(name) {}

  virtual void begin_v() { std::cout << get_name() << ": begin_v" << std::endl; }
  virtual void end_v() { std::cout << get_name() << ": end_v" << std::endl; }

  virtual void visit( uvm_component* node )
  {
    std::cout << get_name() << ": " << node->get_full_name() << std::endl;
  }
};

class leaf : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(leaf);
  leaf( uvm_component_name name ) : uvm_component(name) {}
};

class agent : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(agent);
  agent( uvm_component_name name ) : uvm_component(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_component::build_phase(phase);
    leaf::type_id::create("drv", this);
    leaf::type_id::create("mon", this);
  }
};

class env : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(env);
  env( uvm_component_name name ) : uvm_component(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_component::build_phase(phase);
    agent::type_id::create("a1", this);
    agent::type_id::create("a2", this);
    leaf::type_id::create("sb", this);
  }
};

// a structure which is not a component, with its own proxy
struct node
{
  std::string name;
  std::vector<node*> kids;
  explicit node( const std::string& n ) : name(n) {}
};

class node_proxy : public uvm_structure_proxy<node>
{
 public:
  node_proxy() : uvm_structure_proxy<node>("node_proxy") {}

  virtual void get_immediate_children( node* s, std::vector<node*>& children )
  {
    children.insert(children.end(), s->kids.begin(), s->kids.end());
  }
};

class node_visitor : public uvm_visitor<node>
{
 public:
  node_visitor() : uvm_visitor<node>("node_visitor") {}
  std::string order;
  virtual void visit( node* n ) { order += n->name + " "; }
};

// the name constraint of the name check visitor can be replaced
class strict_name_check_visitor : public uvm_component_name_check_visitor
{
 public:
  strict_name_check_visitor() : uvm_component_name_check_visitor("strict") {}
  virtual std::string get_name_constraint() const { return "^[a-z]+$"; }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    env::type_id::create("env", this);
    leaf::type_id::create("bad+name", this);
  }

  virtual void end_of_elaboration_phase( uvm_phase& phase )
  {
    uvm_component_proxy proxy("proxy");
    uvm_component* e = lookup("env");

    print_visitor td("top_down");
    uvm_top_down_visitor_adapter<> top_down("top_down_adapter");
    top_down.accept(e, &td, &proxy);

    print_visitor bu("bottom_up");
    uvm_bottom_up_visitor_adapter<> bottom_up("bottom_up_adapter");
    bottom_up.accept(e, &bu, &proxy);

    print_visitor bl("by_level");
    uvm_by_level_visitor_adapter<> by_level("by_level_adapter");
    by_level.accept(e, &bl, &proxy);

    // without the begin_v and end_v calls
    print_visitor nb("no_begin_end");
    top_down.accept(lookup("env.a1"), &nb, &proxy, false);

    // through the abstract base classes
    uvm_visitor_adapter<uvm_component>* adapter = &bottom_up;
    uvm_structure_proxy<uvm_component>* sp = &proxy;
    print_visitor base("base");
    adapter->accept(lookup("env.a2"), &base, sp);

    // name checks, starting at the top: only "bad+name" is reported
    uvm_coreservice_t* cs = uvm_coreservice_t::get();
    uvm_component_name_check_visitor check("name-check-visitor");
    std::cout << "name constraint: " << check.get_name_constraint() << std::endl;
    top_down.accept(cs->get_root(), &check, &proxy);

    // a user defined constraint: "a1" and "a2" are reported
    strict_name_check_visitor strict;
    top_down.accept(e, &strict, &proxy);
  }
};

int sc_main(int, char*[])
{
  // traversal of a user defined structure
  node r("r"), a("a"), b("b"), a1("a1"), a2("a2"), b1("b1");
  r.kids.push_back(&a); r.kids.push_back(&b);
  a.kids.push_back(&a1); a.kids.push_back(&a2);
  b.kids.push_back(&b1);

  node_proxy np;
  node_visitor v1, v2, v3;
  uvm_top_down_visitor_adapter<node> td;
  uvm_bottom_up_visitor_adapter<node> bu;
  uvm_by_level_visitor_adapter<node> bl;
  td.accept(&r, &v1, &np);
  bu.accept(&r, &v2, &np);
  bl.accept(&r, &v3, &np);
  std::cout << "node top_down:  " << v1.order << std::endl;
  std::cout << "node bottom_up: " << v2.order << std::endl;
  std::cout << "node by_level:  " << v3.order << std::endl;

  run_test("test");
  return 0;
}
