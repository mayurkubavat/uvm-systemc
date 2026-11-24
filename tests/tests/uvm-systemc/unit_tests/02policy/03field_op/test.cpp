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

// Unit test for IEEE 1800.2-2020 uvm_field_op, uvm_field_op_type,
// and uvm_object::do_execute_op(uvm_field_op*).

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>
#include <vector>

using namespace uvm;

class field_op_catcher : public uvm_report_catcher
{
 public:
  int set_err_count;
  int noset_err_count;

  field_op_catcher() : set_err_count(0), noset_err_count(0) {}

  virtual action_e do_catch()
  {
    const std::string id = get_id();
    if (id == "UVM/FIELD_OP/SET" && get_severity() == UVM_ERROR)
    {
      set_err_count++;
      return CAUGHT;
    }
    if (id == "UVM/FIELD_OP/NO_SET" && get_severity() == UVM_ERROR)
    {
      noset_err_count++;
      return CAUGHT;
    }
    return THROW;
  }
};

class dummy_policy : public uvm_policy
{
 public:
  explicit dummy_policy( const std::string& name = "dummy_policy" ) : uvm_policy(name) {}
  virtual ~dummy_policy() {}
  UVM_OBJECT_UTILS(dummy_policy);
};

class plain_obj : public uvm_object
{
 public:
  explicit plain_obj( const std::string& name = "plain_obj" ) : uvm_object(name) {}
  virtual ~plain_obj() {}
  UVM_OBJECT_UTILS(plain_obj);
};

class hook_disabled_item : public uvm_object
{
 public:
  int val;
  int exec_count;
  int do_copy_count;

  explicit hook_disabled_item( const std::string& name = "hook_disabled_item" )
  : uvm_object(name), val(0), exec_count(0), do_copy_count(0) {}
  virtual ~hook_disabled_item() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    exec_count++;
    if (op != nullptr && op->get_op_type() == UVM_COPY)
    {
      const hook_disabled_item* r = dynamic_cast<const hook_disabled_item*>(op->get_rhs());
      if (r != nullptr)
        val = r->val;
      op->disable_user_hook();
    }
  }

  virtual void do_copy( const uvm_object& rhs )
  {
    do_copy_count++;
    const hook_disabled_item* r = dynamic_cast<const hook_disabled_item*>(&rhs);
    if (r != nullptr)
      val = r->val;
  }

  UVM_OBJECT_UTILS(hook_disabled_item);
};

class hook_enabled_item : public uvm_object
{
 public:
  int a;
  int b;
  int exec_count;
  int do_copy_count;
  std::vector<uvm_field_op_type> seen_ops;

  explicit hook_enabled_item( const std::string& name = "hook_enabled_item" )
  : uvm_object(name), a(0), b(0), exec_count(0), do_copy_count(0) {}
  virtual ~hook_enabled_item() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    exec_count++;
    if (op != nullptr)
    {
      seen_ops.push_back(op->get_op_type());
      if (op->get_op_type() == UVM_COPY)
      {
        const hook_enabled_item* r = dynamic_cast<const hook_enabled_item*>(op->get_rhs());
        if (r != nullptr)
          a = r->a;
      }
    }
  }

  virtual void do_copy( const uvm_object& rhs )
  {
    do_copy_count++;
    const hook_enabled_item* r = dynamic_cast<const hook_enabled_item*>(&rhs);
    if (r != nullptr)
      b = r->b;
  }

  UVM_OBJECT_UTILS(hook_enabled_item);
};

class field_op_test : public uvm_test
{
 public:
  bool all_ok;

  explicit field_op_test( uvm_component_name name )
  : uvm_test(name), all_ok(true) {}

  UVM_COMPONENT_UTILS(field_op_test);

  void check( bool cond, const std::string& desc )
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("FIELD_OP_TEST", "FAILED: " + desc);
    }
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // 1. Factory creation & type identity
    uvm_field_op* fact_op = uvm_field_op::type_id::create("fact_op");
    check(fact_op != nullptr, "uvm_field_op::type_id::create != nullptr");
    check(fact_op != nullptr && fact_op->get_type_name() == "uvm_field_op",
          "uvm_field_op get_type_name()");
    check(fact_op != nullptr && fact_op->get_object_type() == uvm_field_op::get_type(),
          "uvm_field_op get_object_type()");
    uvm_field_op::type_id::destroy(fact_op);

    // 2. All 7 uvm_field_op_type values, set/get/user_hook/disable/flush
    const uvm_field_op_type all_ops[7] = {
      UVM_COPY, UVM_COMPARE, UVM_PRINT, UVM_RECORD, UVM_PACK, UVM_UNPACK, UVM_SET
    };
    dummy_policy pol("pol");
    plain_obj rhs_obj("rhs_obj");
    uvm_field_op op("op");

    for (int i = 0; i < 7; ++i)
    {
      op.set(all_ops[i], &pol, &rhs_obj);
      check(op.get_op_type() == all_ops[i], "get_op_type matches set()");
      check(op.get_policy() == &pol, "get_policy matches set()");
      check(op.get_rhs() == &rhs_obj, "get_rhs matches set()");
      check(op.user_hook_enabled(), "user_hook_enabled is true after set()");
      op.disable_user_hook();
      check(!op.user_hook_enabled(), "user_hook_enabled is false after disable_user_hook()");
      op.flush();
    }

    // Null policy/rhs arguments
    op.set(UVM_SET, nullptr, nullptr);
    check(op.get_op_type() == UVM_SET, "UVM_SET with null policy/rhs");
    check(op.get_policy() == nullptr, "get_policy == nullptr");
    check(op.get_rhs() == nullptr, "get_rhs == nullptr");
    op.flush();

    // 3. Base uvm_object::do_execute_op is a silent no-op across all 7 ops
    plain_obj base_target("base_target");
    for (int i = 0; i < 7; ++i)
    {
      op.set(all_ops[i], &pol, &rhs_obj);
      base_target.do_execute_op(&op);
      check(op.user_hook_enabled(), "base do_execute_op leaves user_hook_enabled true");
      op.flush();
    }

    // 4. Subclass disabling user hook skips do_copy during copy()
    uvm_copier copier("copier");
    hook_disabled_item hd_src("hd_src"), hd_dst("hd_dst");
    hd_src.val = 314;
    hd_dst.copy(hd_src, &copier);
    check(hd_dst.exec_count == 1, "hook_disabled_item do_execute_op called once");
    check(hd_dst.do_copy_count == 0, "hook_disabled_item do_copy skipped when hook disabled");
    check(hd_dst.val == 314, "hook_disabled_item copied val via do_execute_op");

    // 5. Subclass leaving user hook enabled runs both do_execute_op and do_copy
    hook_enabled_item he_src("he_src"), he_dst("he_dst");
    he_src.a = 111;
    he_src.b = 222;
    he_dst.copy(he_src, &copier);
    check(he_dst.exec_count == 1, "hook_enabled_item do_execute_op called once");
    check(he_dst.do_copy_count == 1, "hook_enabled_item do_copy also called");
    check(he_dst.a == 111 && he_dst.b == 222, "both do_execute_op and do_copy copied fields");

    // Direct do_execute_op across all 7 ops
    he_dst.seen_ops.clear();
    for (int i = 0; i < 7; ++i)
    {
      op.set(all_ops[i], &pol, &he_src);
      he_dst.do_execute_op(&op);
      op.flush();
    }
    check(he_dst.seen_ops.size() == 7, "direct do_execute_op saw all 7 op types");
    for (int i = 0; i < 7; ++i)
      check(he_dst.seen_ops[i] == all_ops[i], "direct do_execute_op preserved op order");

    // 6. Error guards: NO_SET before set() / after flush(), SET on double set()
    field_op_catcher ctchr;
    uvm_report_cb::add(nullptr, &ctchr);

    uvm_field_op bad_op("bad_op");
    (void)bad_op.get_op_type();
    check(bad_op.get_policy() == nullptr, "get_policy before set returns nullptr");
    check(bad_op.get_rhs() == nullptr, "get_rhs before set returns nullptr");
    check(!bad_op.user_hook_enabled(), "user_hook_enabled before set returns false");
    bad_op.disable_user_hook();
    check(ctchr.noset_err_count == 5, "5 NO_SET errors caught before set()");

    bad_op.set(UVM_COPY, nullptr, nullptr);
    bad_op.set(UVM_COMPARE, nullptr, nullptr);
    check(ctchr.set_err_count == 1, "1 SET error caught on double set()");

    bad_op.flush();
    (void)bad_op.get_op_type();
    check(ctchr.noset_err_count == 6, "NO_SET error caught after flush()");

    bad_op.set(UVM_PRINT, &pol, nullptr);
    check(bad_op.get_op_type() == UVM_PRINT, "set succeeds cleanly after flush()");

    uvm_report_cb::do_delete(nullptr, &ctchr);

    std::cout << "field_op_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
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
  run_test("field_op_test");
  return 0;
}
