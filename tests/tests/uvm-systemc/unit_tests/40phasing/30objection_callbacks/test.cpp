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

// Test of uvm_objection_callback: callbacks added to an objection with
// uvm_objection_cbs_t are called when objections are raised, dropped
// and all dropped, for the object and for each of its parents.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class my_cb : public uvm_objection_callback
{
 public:
  my_cb( const std::string& name ) : uvm_objection_callback(name) {}

  void show( const std::string& what, uvm_objection* objection, uvm_object* obj,
             uvm_object* source_obj, const std::string& description, int count )
  {
    std::cout << sc_core::sc_time_stamp() << ": " << get_name() << " " << what
              << " at '" << obj->get_full_name() << "' from '"
              << source_obj->get_full_name() << "' count " << count;
    if (!description.empty())
      std::cout << " (" << description << ")";
    std::cout << std::endl;
  }

  virtual void raised( uvm_objection* objection, uvm_object* obj, uvm_object* source_obj,
                       const std::string& description, int count )
  {
    show("raised", objection, obj, source_obj, description, count);
  }

  virtual void dropped( uvm_objection* objection, uvm_object* obj, uvm_object* source_obj,
                        const std::string& description, int count )
  {
    show("dropped", objection, obj, source_obj, description, count);
  }

  virtual void all_dropped( uvm_objection* objection, uvm_object* obj, uvm_object* source_obj,
                            const std::string& description, int count )
  {
    show("all_dropped", objection, obj, source_obj, description, count);
  }
};

class comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(comp);

  comp( uvm_component_name name ) : uvm_component(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this, "busy");
    sc_core::wait(10, sc_core::SC_NS);
    phase.drop_objection(this, "done");
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_objection own;
  my_cb own_cb;
  my_cb disabled_cb;

  test( uvm_component_name name )
  : uvm_test(name), own("own"), own_cb("own_cb"), disabled_cb("disabled_cb") {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    comp::type_id::create("c", this);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // a callback for one objection only
    uvm_objection_cbs_t::add(&own, &own_cb);
    uvm_objection_cbs_t::add(&own, &disabled_cb);
    disabled_cb.callback_mode(0);

    sc_core::wait(20, sc_core::SC_NS);
    own.raise_objection(this, "own objection", 2);
    sc_core::wait(5, sc_core::SC_NS);
    own.drop_objection(this, "own objection", 2);
    sc_core::wait(5, sc_core::SC_NS);

    // removed again
    uvm_objection_cbs_t::do_delete(&own, &own_cb);
    own.raise_objection(this);
    own.drop_objection(this);
    sc_core::wait(5, sc_core::SC_NS);
    std::cout << sc_core::sc_time_stamp() << ": no callback after it was deleted" << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[])
{
  // a callback for every objection
  my_cb all_cb("all_cb");
  uvm_objection_cbs_t::add(nullptr, &all_cb);

  run_test("test");
  return 0;
}
