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

// Unit test for IEEE 1800.2-2020 uvm_policy base class:
// abstractness, active-object stack, cycle checks, extensions, and flush().

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>
#include <type_traits>

using namespace uvm;

class test_policy : public uvm_policy
{
 public:
  explicit test_policy( const std::string& name = "test_policy" )
  : uvm_policy(name), flush_count(0) {}
  virtual ~test_policy() {}

  virtual void flush()
  {
    flush_count++;
    uvm_policy::flush();
  }

  int flush_count;

  UVM_OBJECT_UTILS(test_policy);
};

class dummy_item : public uvm_object
{
 public:
  explicit dummy_item( const std::string& name = "dummy_item" ) : uvm_object(name) {}
  virtual ~dummy_item() {}
  UVM_OBJECT_UTILS(dummy_item);
};

class ext_a : public uvm_object
{
 public:
  explicit ext_a( const std::string& name = "ext_a" ) : uvm_object(name), tag(11) {}
  virtual ~ext_a() {}
  int tag;
  UVM_OBJECT_UTILS(ext_a);
};

class ext_b : public uvm_object
{
 public:
  explicit ext_b( const std::string& name = "ext_b" ) : uvm_object(name), tag(22) {}
  virtual ~ext_b() {}
  int tag;
  UVM_OBJECT_UTILS(ext_b);
};

class policy_test : public uvm_test
{
 public:
  bool all_ok;

  explicit policy_test( uvm_component_name name )
  : uvm_test(name), all_ok(true) {}

  UVM_COMPONENT_UTILS(policy_test);

  void check( bool cond, const std::string& desc )
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("POLICY_TEST", "FAILED: " + desc);
    }
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // 1. Abstractness & factory registration of uvm_policy
    check(!std::is_constructible<uvm_policy>::value,
          "uvm_policy default constructor is not publicly constructible");
    check(!std::is_constructible<uvm_policy, const std::string&>::value,
          "uvm_policy(name) constructor is not publicly constructible");
    check(uvm_policy::get_type() != nullptr, "uvm_policy::get_type() != nullptr");
    check(uvm_policy::get_type() == uvm_abstract_object_registry<uvm_policy>::get(),
          "uvm_policy::get_type() matches uvm_abstract_object_registry<uvm_policy>");

    uvm_policy::type_id::set_type_override(test_policy::get_type());
    uvm_policy* poly_pol = uvm_policy::type_id::create("poly_pol");
    check(poly_pol != nullptr, "uvm_policy abstract factory override creates test_policy");
    check(poly_pol != nullptr && poly_pol->get_type_name() == "test_policy",
          "poly_pol->get_type_name() == test_policy");
    uvm_policy::type_id::destroy(poly_pol);

    test_policy pol("pol");
    check(pol.get_object_type() == test_policy::get_type(),
          "test_policy get_object_type() == test_policy::get_type()");

    // 2. Active-object stack: empty, null guard, nested push/pop, depth, is_active_object
    dummy_item obj_a("obj_a");
    dummy_item obj_b("obj_b");
    dummy_item obj_c("obj_c");

    check(pol.get_active_object() == nullptr, "initial active object is nullptr");
    check(pol.get_active_object_depth() == 0, "initial depth is 0");
    check(pol.pop_active_object() == nullptr, "pop on empty stack returns nullptr");
    check(!pol.is_active_object(nullptr), "is_active_object(nullptr) is false");
    check(!pol.is_active_object(&obj_a), "obj_a not active initially");

    pol.push_active_object(nullptr);
    check(pol.get_active_object() == nullptr, "push(nullptr) ignored");
    check(pol.get_active_object_depth() == 0, "depth stays 0 after push(nullptr)");

    pol.push_active_object(&obj_a);
    check(pol.get_active_object_depth() == 1, "depth is 1 after push(obj_a)");
    check(pol.get_active_object() == &obj_a, "active object is obj_a");
    check(pol.is_active_object(&obj_a), "obj_a is active");
    check(!pol.is_active_object(&obj_b), "obj_b is not active");

    pol.push_active_object(&obj_b);
    check(pol.get_active_object_depth() == 2, "depth is 2 after push(obj_b)");
    check(pol.get_active_object() == &obj_b, "active object is obj_b");
    check(pol.is_active_object(&obj_a), "obj_a still active");
    check(pol.is_active_object(&obj_b), "obj_b is active");
    check(!pol.is_active_object(&obj_c), "obj_c is not active");

    check(pol.pop_active_object() == &obj_b, "pop returns obj_b");
    check(pol.get_active_object_depth() == 1, "depth returns to 1");
    check(pol.get_active_object() == &obj_a, "active object returns to obj_a");
    check(!pol.is_active_object(&obj_b), "obj_b no longer active after pop");
    check(pol.is_active_object(&obj_a), "obj_a still active");

    check(pol.pop_active_object() == &obj_a, "pop returns obj_a");
    check(pol.get_active_object_depth() == 0, "depth returns to 0");
    check(pol.get_active_object() == nullptr, "active object returns to nullptr");
    check(pol.pop_active_object() == nullptr, "extra pop returns nullptr");

    // 3. Extensions (stack + factory-created, overwrite, missing, clear)
    check(!pol.extension_exists(nullptr), "extension_exists(nullptr) is false");
    check(pol.get_extension(nullptr) == nullptr, "get_extension(nullptr) is nullptr");
    check(!pol.extension_exists(ext_a::get_type()), "ext_a absent initially");
    check(pol.get_extension(ext_a::get_type()) == nullptr, "get_extension(ext_a) nullptr");

    ext_a stack_ext1("stack_ext1");
    ext_a stack_ext2("stack_ext2");
    stack_ext2.tag = 99;
    ext_b* fact_ext = ext_b::type_id::create("fact_ext");

    pol.set_extension(nullptr);
    pol.set_extension(&stack_ext1);
    check(pol.extension_exists(ext_a::get_type()), "ext_a exists after set");
    check(pol.get_extension(ext_a::get_type()) == &stack_ext1, "get_extension returns stack_ext1");

    pol.set_extension(&stack_ext2);
    check(pol.get_extension(ext_a::get_type()) == &stack_ext2, "overwrite ext_a returns stack_ext2");

    pol.set_extension(fact_ext);
    check(pol.extension_exists(ext_b::get_type()), "ext_b exists after set");
    check(pol.get_extension(ext_b::get_type()) == fact_ext, "get_extension returns fact_ext");

    // 4. flush() clears stack while preserving extensions
    pol.push_active_object(&obj_a);
    pol.push_active_object(&obj_b);
    check(pol.get_active_object_depth() == 2, "depth is 2 before flush");

    pol.flush();
    check(pol.flush_count == 1, "flush_count == 1");
    check(pol.get_active_object_depth() == 0, "flush clears active stack");
    check(pol.get_active_object() == nullptr, "active object nullptr after flush");
    check(!pol.is_active_object(&obj_a) && !pol.is_active_object(&obj_b),
          "no active objects after flush");
    check(pol.extension_exists(ext_a::get_type()) && pol.get_extension(ext_a::get_type()) == &stack_ext2,
          "flush preserves ext_a");
    check(pol.extension_exists(ext_b::get_type()) && pol.get_extension(ext_b::get_type()) == fact_ext,
          "flush preserves ext_b");

    pol.clear_extension(ext_a::get_type());
    check(!pol.extension_exists(ext_a::get_type()), "clear_extension removes ext_a");
    check(pol.extension_exists(ext_b::get_type()), "clear_extension preserves ext_b");

    pol.clear_extensions();
    check(!pol.extension_exists(ext_b::get_type()), "clear_extensions removes ext_b");

    ext_b::type_id::destroy(fact_ext);

    std::cout << "policy_base_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
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
  run_test("policy_test");
  return 0;
}
