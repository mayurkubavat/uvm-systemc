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

// Unit test for IEEE 1800.2-2020 uvm_comparer:
// uvm_policy inheritance, factory registration, public ctor/dtor,
// get_default/set_default/init, policy initialization & set_policy(),
// compare_object(const uvm_object*), UVM_REFERENCE, check_type,
// cycle/DAG tracking, flush(), and uvm_field_op (UVM_COMPARE).

#include <systemc>
#include <uvm>

#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace uvm;

static bool g_comparer_default_nonnull_before_sc_main = (uvm_default_comparer != nullptr);

class cmp_miscmp_catcher : public uvm_report_catcher
{
 public:
  int miscmp_count;

  cmp_miscmp_catcher() : miscmp_count(0) {}

  virtual action_e do_catch()
  {
    if (get_id() == "MISCMP")
    {
      miscmp_count++;
      return CAUGHT;
    }
    return THROW;
  }
};

class cmp_leaf : public uvm_object
{
 public:
  int val;

  explicit cmp_leaf( const std::string& name = "cmp_leaf" )
  : uvm_object(name), val(0) {}
  virtual ~cmp_leaf() {}

  virtual bool do_compare( const uvm_object& rhs, const uvm_comparer* comparer ) const
  {
    const cmp_leaf* r = dynamic_cast<const cmp_leaf*>(&rhs);
    if (r == nullptr)
      return false;
    return comparer->compare_field_int("val", val, r->val, 32, UVM_DEC);
  }

  UVM_OBJECT_UTILS(cmp_leaf);
};

class cmp_leaf_alt : public cmp_leaf
{
 public:
  explicit cmp_leaf_alt( const std::string& name = "cmp_leaf_alt" )
  : cmp_leaf(name) {}
  virtual ~cmp_leaf_alt() {}
  UVM_OBJECT_UTILS(cmp_leaf_alt);
};

class cmp_node : public uvm_object
{
 public:
  int tag;
  cmp_node* next;

  explicit cmp_node( const std::string& name = "cmp_node" )
  : uvm_object(name), tag(0), next(nullptr) {}
  virtual ~cmp_node() {}

  virtual bool do_compare( const uvm_object& rhs, const uvm_comparer* comparer ) const
  {
    const cmp_node* r = dynamic_cast<const cmp_node*>(&rhs);
    if (r == nullptr)
      return false;
    bool ok = comparer->compare_field_int("tag", tag, r->tag, 32, UVM_DEC);
    ok &= comparer->compare_object("next", next, r->next);
    return ok;
  }

  UVM_OBJECT_UTILS(cmp_node);
};

class cmp_op_item : public uvm_object
{
 public:
  int a;
  int b;
  mutable int exec_count;
  mutable int do_compare_count;
  bool disable_hook;
  mutable bool seen_policy_match;

  explicit cmp_op_item( const std::string& name = "cmp_op_item" )
  : uvm_object(name),
    a(0),
    b(0),
    exec_count(0),
    do_compare_count(0),
    disable_hook(false),
    seen_policy_match(false) {}
  virtual ~cmp_op_item() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    if (op == nullptr || op->get_op_type() != UVM_COMPARE)
      return;
    exec_count++;
    const cmp_op_item* r = dynamic_cast<const cmp_op_item*>(op->get_rhs());
    uvm_comparer* c = dynamic_cast<uvm_comparer*>(op->get_policy());
    if (r == nullptr || c == nullptr)
      return;
    seen_policy_match = (c == op->get_policy());
    c->compare_field_int("a", a, r->a, 32, UVM_DEC);
    if (disable_hook)
      op->disable_user_hook();
  }

  virtual bool do_compare( const uvm_object& rhs, const uvm_comparer* comparer ) const
  {
    do_compare_count++;
    const cmp_op_item* r = dynamic_cast<const cmp_op_item*>(&rhs);
    if (r == nullptr)
      return false;
    return comparer->compare_field_int("b", b, r->b, 32, UVM_DEC);
  }

  UVM_OBJECT_UTILS(cmp_op_item);
};

class cmp_throwing_item : public uvm_object
{
 public:
  bool do_throw;

  explicit cmp_throwing_item( const std::string& name = "cmp_throwing_item" )
  : uvm_object(name), do_throw(false) {}
  virtual ~cmp_throwing_item() {}

  virtual bool do_compare( const uvm_object& rhs, const uvm_comparer* comparer ) const
  {
    (void)rhs;
    (void)comparer;
    if (do_throw)
      throw std::runtime_error("boom_compare");
    return true;
  }

  UVM_OBJECT_UTILS(cmp_throwing_item);
};

class cmp_retry_leaf : public uvm_object
{
 public:
  int val;
  bool do_throw;

  explicit cmp_retry_leaf( const std::string& name = "cmp_retry_leaf" )
  : uvm_object(name), val(0), do_throw(false) {}
  virtual ~cmp_retry_leaf() {}

  virtual bool do_compare( const uvm_object& rhs, const uvm_comparer* comparer ) const
  {
    if (do_throw)
      throw std::runtime_error("boom_retry");
    const cmp_retry_leaf* r = dynamic_cast<const cmp_retry_leaf*>(&rhs);
    if (r == nullptr)
      return false;
    return comparer->compare_field_int("val", val, r->val, 32, UVM_DEC);
  }

  UVM_OBJECT_UTILS(cmp_retry_leaf);
};

class cmp_nested_catch_outer : public uvm_object
{
 public:
  cmp_retry_leaf* child;
  mutable bool caught_inner;
  mutable bool retry_result;

  explicit cmp_nested_catch_outer( const std::string& name = "cmp_nested_catch_outer" )
  : uvm_object(name), child(nullptr), caught_inner(false), retry_result(true) {}
  virtual ~cmp_nested_catch_outer() {}

  virtual bool do_compare( const uvm_object& rhs, const uvm_comparer* comparer ) const
  {
    const cmp_nested_catch_outer* r = dynamic_cast<const cmp_nested_catch_outer*>(&rhs);
    if (r == nullptr || child == nullptr || r->child == nullptr)
      return false;

    try
    {
      (void)comparer->compare_object("child", child, r->child);
    }
    catch (const std::runtime_error&)
    {
      caught_inner = true;
    }

    child->do_throw = false;
    retry_result = comparer->compare_object("child_retry", child, r->child);
    return retry_result;
  }

  UVM_OBJECT_UTILS(cmp_nested_catch_outer);
};

class comparer_test : public uvm_test
{
 public:
  bool all_ok;

  explicit comparer_test( uvm_component_name name )
  : uvm_test(name), all_ok(true) {}

  UVM_COMPONENT_UTILS(comparer_test);

  void check( bool cond, const std::string& desc )
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("COMPARER_TEST", "FAILED: " + desc);
    }
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // 1. Inheritance, constructibility, factory utils, defaults
    check(std::is_base_of<uvm_policy, uvm_comparer>::value,
          "uvm_comparer derives from uvm_policy");
    check(std::is_constructible<uvm_comparer>::value,
          "uvm_comparer is publicly default-constructible");
    check(std::is_constructible<uvm_comparer, const std::string&>::value,
          "uvm_comparer(name) is publicly constructible");
    check(g_comparer_default_nonnull_before_sc_main,
          "uvm_default_comparer non-null before sc_main");
    check(uvm_comparer::get_default() != nullptr,
          "uvm_comparer::get_default() != nullptr");
    check(uvm_comparer::init() == uvm_comparer::get_default(),
          "uvm_comparer::init() == get_default()");
    check(uvm_default_comparer == uvm_comparer::get_default(),
          "uvm_default_comparer == get_default()");

    uvm_comparer* fact_c = uvm_comparer::type_id::create("fact_c");
    check(fact_c != nullptr, "uvm_comparer::type_id::create != nullptr");
    check(fact_c != nullptr && fact_c->get_type_name() == "uvm_comparer",
          "uvm_comparer get_type_name()");
    check(fact_c != nullptr && fact_c->get_object_type() == uvm_comparer::get_type(),
          "uvm_comparer get_object_type()");

    uvm_comparer* orig_def = uvm_comparer::get_default();
    uvm_comparer::set_default(fact_c);
    check(uvm_comparer::get_default() == fact_c && uvm_default_comparer == fact_c,
          "set_default updates get_default() and uvm_default_comparer");
    uvm_comparer::set_default(nullptr);
    check(uvm_comparer::get_default() == orig_def && uvm_default_comparer == orig_def,
          "set_default(nullptr) restores builtin default");
    uvm_comparer::type_id::destroy(fact_c);

    // 2. Policy initialization & set_policy()
    uvm_comparer cmp("cmp");
    check(cmp.get_policy() == UVM_DEFAULT_POLICY && cmp.policy == UVM_DEFAULT_POLICY,
          "uvm_comparer::policy initialized to UVM_DEFAULT_POLICY");
    cmp.set_policy(UVM_REFERENCE);
    check(cmp.get_policy() == UVM_REFERENCE && cmp.policy == UVM_REFERENCE,
          "set_policy(UVM_REFERENCE) works");
    cmp.set_policy(UVM_SHALLOW);
    check(cmp.get_policy() == UVM_SHALLOW && cmp.policy == UVM_SHALLOW,
          "set_policy(UVM_SHALLOW) works");
    cmp.set_policy(UVM_DEEP);
    check(cmp.get_policy() == UVM_DEEP && cmp.policy == UVM_DEEP,
          "set_policy(UVM_DEEP) works");

    cmp_miscmp_catcher ctchr;
    uvm_report_cb::add(nullptr, &ctchr);

    // 3. Pointer compare_object, UVM_REFERENCE, check_type, and default comparer isolation
    cmp_leaf l1("l1"), l2("l2");
    l1.val = 77;
    l2.val = 77;
    uvm_comparer::get_default()->flush();

    cmp.set_policy(UVM_REFERENCE);
    check(cmp.compare_object("same", &l1, &l1), "UVM_REFERENCE same pointer returns true");
    check(!cmp.compare_object("diff_ref", &l1, &l2),
          "UVM_REFERENCE distinct equal objects returns false");
    check(cmp.get_result() == 1, "UVM_REFERENCE increments custom comparer result");
    check(uvm_comparer::get_default()->get_result() == 0,
          "UVM_REFERENCE on custom comparer does not mutate uvm_default_comparer");

    cmp.flush();
    check(cmp.get_result() == 0 && cmp.get_miscompare_string().empty(),
          "flush clears result and miscompares");
    cmp.set_policy(UVM_DEEP);

    check(cmp.compare_object("both_null",
                             static_cast<const uvm_object*>(nullptr),
                             static_cast<const uvm_object*>(nullptr)),
          "compare_object(nullptr, nullptr) returns true");
    check(!cmp.compare_object("lhs_null", nullptr, &l2),
          "compare_object(nullptr, &l2) returns false");
    check(cmp.get_result() == 1, "null mismatch increments result");
    cmp.flush();
    check(!cmp.compare_object("rhs_null", &l1, nullptr),
          "compare_object(&l1, nullptr) returns false");
    check(cmp.get_result() == 1, "rhs null mismatch increments result");

    // check_type
    cmp_leaf_alt alt("alt");
    alt.val = 77;
    cmp.flush();
    cmp.compare_type(true);
    check(!cmp.compare_object("type_chk_on", &l1, &alt),
          "check_type=true rejects distinct types with equal fields");
    cmp.flush();
    cmp.compare_type(false);
    check(cmp.compare_object("type_chk_off", &l1, &alt),
          "check_type=false allows derived/base comparison");

    // 4. Cycle / DAG comparison
    cmp_node a1("a1"), b1("b1"), a2("a2"), b2("b2");
    a1.tag = 1; b1.tag = 2; a1.next = &b1; b1.next = &a1;
    a2.tag = 1; b2.tag = 2; a2.next = &b2; b2.next = &a2;

    cmp.flush();
    cmp.compare_type(true);
    check(a1.compare(a2, &cmp), "isomorphic cyclic graph compares true");
    check(cmp.get_result() == 0, "isomorphic cyclic graph has 0 miscompares");

    b2.tag = 99;
    check(!a1.compare(a2, &cmp), "non-isomorphic cyclic graph compares false");
    check(cmp.get_result() == 1, "non-isomorphic cyclic graph reports 1 miscompare");
    check(!cmp.get_miscompare_string().empty(),
          "miscompare string remains inspectable after top-level compare()");

    // 5. uvm_field_op (UVM_COMPARE) integration & exception safety
    cmp_op_item o1("o1"), o2("o2");
    o1.a = 10; o1.b = 20;
    o2.a = 10; o2.b = 20;
    cmp.flush();
    check(o1.compare(o2, &cmp), "cmp_op_item matching compare returns true");
    check(o1.exec_count == 1 && o1.do_compare_count == 1 && o1.seen_policy_match,
          "UVM_COMPARE invokes do_execute_op and do_compare");

    o2.b = 999;
    check(!o1.compare(o2, &cmp), "cmp_op_item differing b fails via do_compare");

    o1.disable_hook = true;
    o1.exec_count = 0;
    o1.do_compare_count = 0;
    check(o1.compare(o2, &cmp), "disable_user_hook skips do_compare (only a compared)");
    check(o1.exec_count == 1 && o1.do_compare_count == 0,
          "do_compare skipped when hook disabled");

    o2.a = 888;
    check(!o1.compare(o2, &cmp), "do_execute_op field miscompare fails compare()");

    cmp_throwing_item t1("t1"), t2("t2");
    t1.do_throw = true;
    bool caught = false;
    try
    {
      (void)t1.compare(t2, &cmp);
    }
    catch (const std::runtime_error&)
    {
      caught = true;
    }
    check(caught, "compare() propagates exception from do_compare");
    check(cmp.get_active_object() == nullptr,
          "active object stack cleaned up after compare() exception");

    // 6. Direct compare_object() accumulates result/miscompares & scopes m_compare_map
    {
      cmp.flush();
      cmp.compare_type(true);
      cmp.set_policy(UVM_DEEP);

      check(!cmp.compare_field_int("pre_mismatch", 1, 2, 32, UVM_DEC),
            "pre_mismatch fails");
      check(cmp.get_result() == 1, "pre_mismatch increments result to 1");

      cmp_leaf x1("x1"), x2("x2");
      x1.val = 10;
      x2.val = 10;
      check(cmp.compare_object("x_match", &x1, &x2),
            "direct compare_object(match) returns true");
      check(cmp.get_result() == 1,
            "direct compare_object(match) preserves prior compare_field_int result");
      check(cmp.get_miscompare_string().find("pre_mismatch") != std::string::npos,
            "direct compare_object(match) preserves prior miscompares string");

      cmp_leaf y1("y1"), y2("y2");
      y1.val = 1;
      y2.val = 2;
      check(!cmp.compare_object("y_mismatch", &y1, &y2),
            "direct compare_object(mismatch) returns false");
      check(cmp.get_result() == 2,
            "direct compare_object(mismatch) accumulates result to 2");

      check(cmp.compare_object("x_match_2", &x1, &x2),
            "second direct compare_object(match) returns true");
      check(cmp.get_result() == 2,
            "second direct compare_object(match) preserves prior result == 2");

      // Sequential non-nested compare_object on same pair after mutation must NOT skip as cycle
      x2.val = 99;
      check(!cmp.compare_object("x_mutated", &x1, &x2),
            "sequential compare_object on same pair detects post-mutation mismatch");
      check(cmp.get_result() == 3,
            "result accumulates to 3 after sequential compare_object mismatch");

      // Top-level uvm_object::compare() followed by direct compare_object() on same pair
      cmp_node s1("s1"), s2("s2");
      s1.tag = 5;
      s2.tag = 5;
      check(s1.compare(s2, &cmp), "top-level s1.compare(s2) flushes and returns true");
      check(cmp.get_result() == 0, "top-level s1.compare(s2) resets result to 0");
      s2.tag = 6;
      check(!cmp.compare_object("s_after_top", &s1, &s2),
            "direct compare_object after top-level compare does not hit stale m_compare_map");
      check(cmp.get_result() == 1, "direct compare_object after top-level compare counts mismatch");
    }

    // 7. Caught nested do_compare exception does not leave stale (lhs, rhs) in m_compare_map
    {
      cmp_retry_leaf rl1("rl1"), rl2("rl2");
      rl1.val = 1;
      rl2.val = 2;
      rl1.do_throw = true;

      cmp_nested_catch_outer out1("out1"), out2("out2");
      out1.child = &rl1;
      out2.child = &rl2;

      cmp.flush();
      check(!out1.compare(out2, &cmp),
            "outer compare returns false when retried child mismatches after caught throw");
      check(out1.caught_inner, "outer do_compare caught inner throw");
      check(!out1.retry_result,
            "retry compare_object after caught throw does not falsely return true");
      check(cmp.get_result() == 1, "retry recorded 1 miscompare");
    }

    uvm_report_cb::do_delete(nullptr, &ctchr);
    std::cout << "comparer_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
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
  run_test("comparer_test");
  return 0;
}
