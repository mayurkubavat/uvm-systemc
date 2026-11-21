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

// Unit test for IEEE 1800.2-2020 abstract factory registries,
// UVM_OBJECT_*_ABSTRACT_UTILS / UVM_COMPONENT_*_ABSTRACT_UTILS macros,
// and polymorphic get_object_type() overrides.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

// Catcher to verify FCTTYP (unoverridden abstract type_id::create) and
// NO_OVERRIDE (obj->create() without manual override) cleanly.
class abs_report_catcher : public uvm_report_catcher
{
 public:
  int fcttyp_count;
  int no_override_count;

  abs_report_catcher() : fcttyp_count(0), no_override_count(0) {}

  virtual action_e do_catch()
  {
    const std::string id = get_id();
    if (id == "FCTTYP" && get_severity() == UVM_FATAL)
    {
      fcttyp_count++;
      return CAUGHT;
    }
    if (id == "NO_OVERRIDE" && get_severity() == UVM_ERROR)
    {
      no_override_count++;
      return CAUGHT;
    }
    return THROW;
  }
};

// Abstract uvm_object with pure virtual method (non-constructible via new)
class abs_item : public uvm_object
{
 public:
  explicit abs_item( const std::string& name = "abs_item" ) : uvm_object(name) {}
  virtual ~abs_item() {}

  virtual int compute() const = 0;

  UVM_OBJECT_ABSTRACT_UTILS(abs_item)
};

class conc_item_a : public abs_item
{
 public:
  explicit conc_item_a( const std::string& name = "conc_item_a" ) : abs_item(name) {}
  virtual ~conc_item_a() {}

  virtual int compute() const { return 101; }

  UVM_OBJECT_UTILS(conc_item_a);
};

class conc_item_b : public abs_item
{
 public:
  explicit conc_item_b( const std::string& name = "conc_item_b" ) : abs_item(name) {}
  virtual ~conc_item_b() {}

  virtual int compute() const { return 202; }

  UVM_OBJECT_UTILS(conc_item_b);
};

// Parameterized abstract uvm_object with 2 template arguments
template <typename A, typename B>
class abs_param_item : public uvm_object
{
 public:
  explicit abs_param_item( const std::string& name = "abs_param_item" ) : uvm_object(name) {}
  virtual ~abs_param_item() {}

  virtual A get_val() const = 0;

  UVM_OBJECT_PARAM_ABSTRACT_UTILS(abs_param_item<A, B>);
};

template <typename A, typename B>
class conc_param_item : public abs_param_item<A, B>
{
 public:
  explicit conc_param_item( const std::string& name = "conc_param_item" )
  : abs_param_item<A, B>(name) {}
  virtual ~conc_param_item() {}

  virtual A get_val() const { return static_cast<A>(77); }

  UVM_OBJECT_PARAM_UTILS(conc_param_item<A, B>);
};

// Abstract uvm_component with pure virtual method
class abs_comp : public uvm_component
{
 public:
  explicit abs_comp( uvm_component_name name ) : uvm_component(name) {}
  virtual ~abs_comp() {}

  virtual std::string role() const = 0;

  UVM_COMPONENT_ABSTRACT_UTILS(abs_comp);
};

class conc_comp_a : public abs_comp
{
 public:
  explicit conc_comp_a( uvm_component_name name ) : abs_comp(name) {}
  virtual ~conc_comp_a() {}

  virtual std::string role() const { return "A"; }

  UVM_COMPONENT_UTILS(conc_comp_a);
};

class conc_comp_b : public abs_comp
{
 public:
  explicit conc_comp_b( uvm_component_name name ) : abs_comp(name) {}
  virtual ~conc_comp_b() {}

  virtual std::string role() const { return "B"; }

  UVM_COMPONENT_UTILS(conc_comp_b);
};

// Parameterized abstract uvm_component with 2 template arguments
template <typename A, typename B>
class abs_param_comp : public uvm_component
{
 public:
  explicit abs_param_comp( uvm_component_name name ) : uvm_component(name) {}
  virtual ~abs_param_comp() {}

  virtual int id_val() const = 0;

  UVM_COMPONENT_PARAM_ABSTRACT_UTILS(abs_param_comp<A, B>);
};

template <typename A, typename B>
class conc_param_comp : public abs_param_comp<A, B>
{
 public:
  explicit conc_param_comp( uvm_component_name name ) : abs_param_comp<A, B>(name) {}
  virtual ~conc_param_comp() {}

  virtual int id_val() const { return 55; }

  UVM_COMPONENT_PARAM_UTILS(conc_param_comp<A, B>);
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  bool all_ok;

  explicit test( uvm_component_name name ) : uvm_test(name), all_ok(true) {}

  void check( bool cond, const std::string& label )
  {
    if (!cond)
    {
      all_ok = false;
      std::cout << "FAIL: " << label << std::endl;
    }
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    uvm_factory* factory = uvm_factory::get();
    abs_report_catcher ctchr;
    uvm_report_cb::add(nullptr, &ctchr);

    // 1. Verify abstract registry identity & non-null wrapper
    check(abs_item::get_type() != nullptr, "abs_item::get_type() non-null");
    check(abs_item::get_type() == uvm_abstract_object_registry<abs_item>::get(),
          "abs_item registry singleton match");
    check(abs_comp::get_type() != nullptr, "abs_comp::get_type() non-null");
    check(abs_comp::get_type() == uvm_abstract_component_registry<abs_comp>::get(),
          "abs_comp registry singleton match");
    check(factory->is_type_registered(abs_item::get_type()), "abs_item registered");
    check(factory->is_type_registered(abs_comp::get_type()), "abs_comp registered");

    // 2. Unoverridden abstract creation returns nullptr cleanly
    uvm_object* raw_obj = factory->create_object_by_type(abs_item::get_type(), "", "raw_obj");
    check(raw_obj == nullptr, "create_object_by_type(abs_item) returns nullptr");

    uvm_component* raw_comp = factory->create_component_by_type(abs_comp::get_type(), "", "raw_comp", nullptr);
    check(raw_comp == nullptr, "create_component_by_type(abs_comp) returns nullptr");

    uvm_object* raw_obj_name = factory->create_object_by_name("abs_item", "", "raw_obj_name");
    check(raw_obj_name == nullptr, "create_object_by_name(abs_item) returns nullptr");

    uvm_component* raw_comp_name = factory->create_component_by_name("abs_comp", "", "raw_comp_name", nullptr);
    check(raw_comp_name == nullptr, "create_component_by_name(abs_comp) returns nullptr");

    abs_item* fail_item = abs_item::type_id::create("fail_item");
    check(fail_item == nullptr && ctchr.fcttyp_count == 1,
          "unoverridden abs_item::type_id::create emits FCTTYP and returns nullptr");

    abs_comp* fail_comp = abs_comp::type_id::create("fail_comp", this);
    check(fail_comp == nullptr && ctchr.fcttyp_count == 2,
          "unoverridden abs_comp::type_id::create emits FCTTYP and returns nullptr");

    // 3. Polymorphic get_object_type() and obj->create() (NO_OVERRIDE without compile error)
    abs_item* poly_obj = new conc_item_a("poly_obj");
    check(poly_obj->get_object_type() == conc_item_a::get_type(),
          "poly_obj->get_object_type() == conc_item_a::get_type()");
    check(poly_obj->get_object_type() != abs_item::get_type(),
          "poly_obj->get_object_type() != abs_item::get_type()");

    uvm_object* no_ovr = poly_obj->create("no_ovr");
    check(no_ovr == nullptr && ctchr.no_override_count == 1,
          "obj->create() without manual create() emits NO_OVERRIDE and returns nullptr");
    delete poly_obj;

    conc_comp_a poly_comp("poly_comp");
    abs_comp* poly_comp_base = &poly_comp;
    check(poly_comp_base->get_object_type() == conc_comp_a::get_type(),
          "poly_comp->get_object_type() == conc_comp_a::get_type()");
    check(poly_comp_base->get_object_type() != abs_comp::get_type(),
          "poly_comp->get_object_type() != abs_comp::get_type()");

    uvm_report_cb::do_delete(nullptr, &ctchr);

    // 4. Abstract object type_override + inst_override (with parent==nullptr and parent!=nullptr)
    abs_item::type_id::set_type_override(conc_item_a::get_type());
    abs_item::type_id::set_inst_override(conc_item_b::get_type(), "test.sub_b");
    abs_item::type_id::set_inst_override(conc_item_b::get_type(), "rel_b", this);

    abs_item* i_type = abs_item::type_id::create("i_type", this);
    check(i_type != nullptr && i_type->compute() == 101, "abs_item type override -> conc_item_a");
    check(i_type != nullptr && i_type->get_object_type() == conc_item_a::get_type(),
          "i_type get_object_type() == conc_item_a");

    abs_item* i_inst_abs = abs_item::type_id::create("sub_b", this);
    check(i_inst_abs != nullptr && i_inst_abs->compute() == 202,
          "abs_item inst override (parent==nullptr) -> conc_item_b");

    abs_item* i_inst_rel = abs_item::type_id::create("rel_b", this);
    check(i_inst_rel != nullptr && i_inst_rel->compute() == 202,
          "abs_item inst override (parent!=nullptr) -> conc_item_b");

    uvm_object* i_byname_raw = factory->create_object_by_name("abs_item", get_full_name(), "i_byname");
    abs_item* i_byname = dynamic_cast<abs_item*>(i_byname_raw);
    check(i_byname != nullptr && i_byname->compute() == 101,
          "create_object_by_name(abs_item) -> conc_item_a");

    abs_item::type_id::destroy(i_type);
    abs_item::type_id::destroy(i_inst_abs);
    abs_item::type_id::destroy(i_inst_rel);
    abs_item::type_id::destroy(dynamic_cast<abs_item*>(i_byname_raw));

    // 5. Parameterized abstract object
    abs_param_item<int, double>::type_id::set_type_override(conc_param_item<int, double>::get_type());
    abs_param_item<int, double>* p_item = abs_param_item<int, double>::type_id::create("p_item");
    check(p_item != nullptr && p_item->get_val() == 77,
          "abs_param_item<int, double> type override -> conc_param_item");
    abs_param_item<int, double>::type_id::destroy(p_item);

    // 6. Abstract component type_override + inst_override + create_component_by_name
    abs_comp::type_id::set_type_override(conc_comp_a::get_type());
    abs_comp::type_id::set_inst_override(conc_comp_b::get_type(), "test.c_abs_inst");
    abs_comp::type_id::set_inst_override(conc_comp_b::get_type(), "c_rel_inst", this);

    abs_comp* c_type = abs_comp::type_id::create("c_type", this);
    check(c_type != nullptr && c_type->role() == "A", "abs_comp type override -> conc_comp_a");
    check(c_type != nullptr && c_type->get_object_type() == conc_comp_a::get_type(),
          "c_type get_object_type() == conc_comp_a");

    abs_comp* c_inst1 = abs_comp::type_id::create("c_abs_inst", this);
    check(c_inst1 != nullptr && c_inst1->role() == "B",
          "abs_comp inst override (parent==nullptr) -> conc_comp_b");

    abs_comp* c_inst2 = abs_comp::type_id::create("c_rel_inst", this);
    check(c_inst2 != nullptr && c_inst2->role() == "B",
          "abs_comp inst override (parent!=nullptr) -> conc_comp_b");

    uvm_component* c_byname_raw = factory->create_component_by_name("abs_comp", get_full_name(), "c_byname", this);
    abs_comp* c_byname = dynamic_cast<abs_comp*>(c_byname_raw);
    check(c_byname != nullptr && c_byname->role() == "A",
          "create_component_by_name(abs_comp) -> conc_comp_a");

    // 7. Parameterized abstract component
    abs_param_comp<int, int>::type_id::set_type_override(conc_param_comp<int, int>::get_type());
    abs_param_comp<int, int>* p_comp = abs_param_comp<int, int>::type_id::create("p_comp", this);
    check(p_comp != nullptr && p_comp->id_val() == 55,
          "abs_param_comp<int, int> type override -> conc_param_comp");

    std::cout << "abstract_registry_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    uvm_test::report_phase(phase);
    uvm_report_server* svr = uvm_coreservice_t::get()->get_report_server();
    if (all_ok &&
        svr->get_severity_count(UVM_WARNING) == 0 &&
        svr->get_severity_count(UVM_ERROR) == 0 &&
        svr->get_severity_count(UVM_FATAL) == 0)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "!! UVM TEST FAILED !!" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
