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

// Unit test for IEEE 1800.2-2020 uvm_copier and uvm_object::copy(rhs, copier):
// default copier, recursion policies, cycles, diamond DAG, tracking, flush,
// and recursive 1-arg copy() inside do_copy().

#include <systemc>
#include <uvm>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace uvm;

class leaf_item : public uvm_object
{
 public:
  int id;

  explicit leaf_item( const std::string& name = "leaf_item" )
  : uvm_object(name), id(0) {}
  virtual ~leaf_item() {}

  virtual void do_copy( const uvm_object& rhs )
  {
    const leaf_item* r = dynamic_cast<const leaf_item*>(&rhs);
    if (r != nullptr)
      id = r->id;
  }

  UVM_OBJECT_UTILS(leaf_item);
};

class manual_create_item : public uvm_object
{
 public:
  int code;

  explicit manual_create_item( const std::string& name = "manual_create_item" )
  : uvm_object(name), code(0) {}
  virtual ~manual_create_item() {}

  virtual uvm_object* create( const std::string& name = "" )
  {
    return new manual_create_item(name);
  }

  virtual void do_copy( const uvm_object& rhs )
  {
    const manual_create_item* r = dynamic_cast<const manual_create_item*>(&rhs);
    if (r != nullptr)
      code = r->code;
  }
};

class parent_item : public uvm_object
{
 public:
  int tag;
  leaf_item* child;
  uvm_copier* active_copier_in_do_copy;
  uvm_object* active_obj_in_do_copy;
  unsigned int active_depth_in_do_copy;
  bool owned_child;

  explicit parent_item( const std::string& name = "parent_item" )
  : uvm_object(name),
    tag(0),
    child(nullptr),
    active_copier_in_do_copy(nullptr),
    active_obj_in_do_copy(nullptr),
    active_depth_in_do_copy(0),
    owned_child(false) {}

  virtual ~parent_item()
  {
    if (owned_child && child != nullptr)
      delete child;
  }

  virtual void do_execute_op( uvm_field_op* op )
  {
    if (op != nullptr && op->get_op_type() == UVM_COPY)
      active_copier_in_do_copy = dynamic_cast<uvm_copier*>(op->get_policy());
  }

  virtual void do_copy( const uvm_object& rhs )
  {
    const parent_item* r = dynamic_cast<const parent_item*>(&rhs);
    if (r == nullptr)
      return;

    tag = r->tag;
    uvm_copier* c = (active_copier_in_do_copy != nullptr)
                    ? active_copier_in_do_copy
                    : uvm_copier::get_default();
    active_obj_in_do_copy = c->get_active_object();
    active_depth_in_do_copy = c->get_active_object_depth();

    if (owned_child && child != nullptr)
    {
      delete child;
      child = nullptr;
      owned_child = false;
    }

    if (r->child != nullptr)
    {
      c->copy_object(child, r->child);
      if (child != nullptr && child != r->child)
        owned_child = true;
    }
  }

  UVM_OBJECT_UTILS(parent_item);
};

class node_item : public uvm_object
{
 public:
  int val;
  node_item* next;

  explicit node_item( const std::string& name = "node_item" )
  : uvm_object(name), val(0), next(nullptr) {}
  virtual ~node_item() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    if (op == nullptr || op->get_op_type() != UVM_COPY)
      return;
    const node_item* r = dynamic_cast<const node_item*>(op->get_rhs());
    uvm_copier* c = dynamic_cast<uvm_copier*>(op->get_policy());
    if (r == nullptr || c == nullptr)
      return;

    val = r->val;
    if (r->next != nullptr)
      c->copy_object(next, r->next);
    op->disable_user_hook();
  }

  UVM_OBJECT_UTILS(node_item);
};

class dag_branch : public uvm_object
{
 public:
  leaf_item* leaf;

  explicit dag_branch( const std::string& name = "dag_branch" )
  : uvm_object(name), leaf(nullptr) {}
  virtual ~dag_branch() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    if (op == nullptr || op->get_op_type() != UVM_COPY)
      return;
    const dag_branch* r = dynamic_cast<const dag_branch*>(op->get_rhs());
    uvm_copier* c = dynamic_cast<uvm_copier*>(op->get_policy());
    if (r == nullptr || c == nullptr)
      return;

    if (r->leaf != nullptr)
      c->copy_object(leaf, r->leaf);
    op->disable_user_hook();
  }

  UVM_OBJECT_UTILS(dag_branch);
};

class dag_root : public uvm_object
{
 public:
  dag_branch* left;
  dag_branch* right;

  explicit dag_root( const std::string& name = "dag_root" )
  : uvm_object(name), left(nullptr), right(nullptr) {}
  virtual ~dag_root() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    if (op == nullptr || op->get_op_type() != UVM_COPY)
      return;
    const dag_root* r = dynamic_cast<const dag_root*>(op->get_rhs());
    uvm_copier* c = dynamic_cast<uvm_copier*>(op->get_policy());
    if (r == nullptr || c == nullptr)
      return;

    if (r->left != nullptr)
      c->copy_object(left, r->left);
    if (r->right != nullptr)
      c->copy_object(right, r->right);
    op->disable_user_hook();
  }

  UVM_OBJECT_UTILS(dag_root);
};

class composite_item : public uvm_object
{
 public:
  int outer_val;
  leaf_item embedded;
  bool active_present_before_inner;
  bool active_present_after_inner;

  explicit composite_item( const std::string& name = "composite_item" )
  : uvm_object(name),
    outer_val(0),
    embedded("embedded"),
    active_present_before_inner(false),
    active_present_after_inner(false) {}
  virtual ~composite_item() {}

  virtual void do_copy( const uvm_object& rhs )
  {
    const composite_item* r = dynamic_cast<const composite_item*>(&rhs);
    if (r == nullptr)
      return;
    outer_val = r->outer_val;
    uvm_copier* def = uvm_copier::get_default();
    active_present_before_inner = (def->get_active_object() == this);
    // Recursive 1-arg copy() inside do_copy must NOT flush default copier prematurely
    embedded.copy(r->embedded);
    active_present_after_inner = (def->get_active_object() == this &&
                                  def->get_active_object_depth() == 1);
  }

  UVM_OBJECT_UTILS(composite_item);
};

class throwing_leaf_item : public uvm_object
{
 public:
  int val;
  bool throw_in_do_execute_op;
  bool throw_in_do_copy;

  explicit throwing_leaf_item( const std::string& name = "throwing_leaf_item" )
  : uvm_object(name),
    val(0),
    throw_in_do_execute_op(false),
    throw_in_do_copy(false) {}
  virtual ~throwing_leaf_item() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    (void)op;
    if (throw_in_do_execute_op)
      throw std::runtime_error("boom_execute_op");
  }

  virtual void do_copy( const uvm_object& rhs )
  {
    const throwing_leaf_item* r = dynamic_cast<const throwing_leaf_item*>(&rhs);
    if (throw_in_do_copy || (r != nullptr && r->val == -999))
      throw std::runtime_error("boom_copy");
    if (r != nullptr)
      val = r->val;
  }

  UVM_OBJECT_UTILS(throwing_leaf_item);
};

class throwing_parent_item : public uvm_object
{
 public:
  int pval;
  throwing_leaf_item child;
  uvm_copier* active_copier;

  explicit throwing_parent_item( const std::string& name = "throwing_parent_item" )
  : uvm_object(name),
    pval(0),
    child("child"),
    active_copier(nullptr) {}
  virtual ~throwing_parent_item() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    if (op != nullptr && op->get_op_type() == UVM_COPY)
      active_copier = dynamic_cast<uvm_copier*>(op->get_policy());
  }

  virtual void do_copy( const uvm_object& rhs )
  {
    const throwing_parent_item* r = dynamic_cast<const throwing_parent_item*>(&rhs);
    if (r == nullptr)
      return;
    pval = r->pval;
    uvm_copier* c = (active_copier != nullptr)
                    ? active_copier
                    : uvm_copier::get_default();
    c->copy_object(child, r->child);
  }

  UVM_OBJECT_UTILS(throwing_parent_item);
};

class ovr_base_item : public uvm_object
{
 public:
  int base_val;

  explicit ovr_base_item( const std::string& name = "ovr_base_item" )
  : uvm_object(name), base_val(0) {}
  virtual ~ovr_base_item() {}

  virtual void do_copy( const uvm_object& rhs )
  {
    const ovr_base_item* r = dynamic_cast<const ovr_base_item*>(&rhs);
    if (r != nullptr)
      base_val = r->base_val;
  }

  UVM_OBJECT_UTILS(ovr_base_item);
};

class ovr_derived_item : public ovr_base_item
{
 public:
  int derived_tag;

  explicit ovr_derived_item( const std::string& name = "ovr_derived_item" )
  : ovr_base_item(name), derived_tag(777) {}
  virtual ~ovr_derived_item() {}

  virtual void do_copy( const uvm_object& rhs )
  {
    ovr_base_item::do_copy(rhs);
    const ovr_derived_item* r = dynamic_cast<const ovr_derived_item*>(&rhs);
    if (r != nullptr)
      derived_tag = r->derived_tag;
  }

  UVM_OBJECT_UTILS(ovr_derived_item);
};

class copier_test : public uvm_test
{
 public:
  bool all_ok;

  explicit copier_test( uvm_component_name name )
  : uvm_test(name), all_ok(true) {}

  UVM_COMPONENT_UTILS(copier_test);

  void check( bool cond, const std::string& desc )
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("COPIER_TEST", "FAILED: " + desc);
    }
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // 1. Default copier, factory creation, set_default, recursion policy
    uvm_copier* init_def = uvm_copier::get_default();
    check(init_def != nullptr, "uvm_copier::get_default() != nullptr");
    check(uvm_default_copier == init_def, "uvm_default_copier == get_default()");
    check(init_def->get_recursion_policy() == UVM_DEFAULT_POLICY,
          "default recursion policy is UVM_DEFAULT_POLICY");

    uvm_copier* fact_c = uvm_copier::type_id::create("fact_c");
    check(fact_c != nullptr, "uvm_copier factory create");
    check(fact_c != nullptr && fact_c->get_type_name() == "uvm_copier",
          "uvm_copier type name");
    check(fact_c != nullptr && fact_c->get_object_type() == uvm_copier::get_type(),
          "uvm_copier get_object_type()");

    uvm_copier::set_default(fact_c);
    check(uvm_copier::get_default() == fact_c, "set_default updates get_default()");
    check(uvm_default_copier == fact_c, "set_default updates uvm_default_copier");

    uvm_copier::set_default(nullptr);
    check(uvm_copier::get_default() == init_def, "set_default(nullptr) restores built-in");
    uvm_copier::type_id::destroy(fact_c);

    uvm_copier pol_c("pol_c");
    pol_c.set_recursion_policy(UVM_DEEP);
    check(pol_c.get_recursion_policy() == UVM_DEEP, "set/get UVM_DEEP");
    pol_c.set_recursion_policy(UVM_SHALLOW);
    check(pol_c.get_recursion_policy() == UVM_SHALLOW, "set/get UVM_SHALLOW");
    pol_c.set_recursion_policy(UVM_REFERENCE);
    check(pol_c.get_recursion_policy() == UVM_REFERENCE, "set/get UVM_REFERENCE");
    pol_c.set_recursion_policy(UVM_DEFAULT_POLICY);
    check(pol_c.get_recursion_policy() == UVM_DEFAULT_POLICY, "set/get UVM_DEFAULT_POLICY");

    // 2. Null rhs, self lhs==rhs, null lhs allocation (factory + manual create fallback)
    uvm_copier alloc_c("alloc_c");
    leaf_item l_src("l_src");
    l_src.id = 42;

    uvm_object* ptr_out = &l_src;
    alloc_c.copy_object(ptr_out, nullptr);
    check(ptr_out == nullptr, "copy_object(lhs, nullptr) sets lhs = nullptr");

    leaf_item* self_ptr = &l_src;
    alloc_c.copy_object(self_ptr, &l_src);
    check(self_ptr == &l_src, "copy_object(p, p) leaves pointer unchanged");
    check(alloc_c.get_num_copies(&l_src) == 0, "self copy_object records 0 copies");

    leaf_item* l_clone = nullptr;
    alloc_c.copy_object(l_clone, &l_src);
    check(l_clone != nullptr && l_clone != &l_src, "copy_object allocates new leaf_item");
    check(l_clone != nullptr && l_clone->id == 42, "copy_object copies leaf_item field");
    delete l_clone;

    manual_create_item m_src("m_src");
    m_src.code = 777;
    manual_create_item* m_clone = nullptr;
    alloc_c.copy_object(m_clone, &m_src);
    check(m_clone != nullptr && m_clone != &m_src, "copy_object fallback to create()");
    check(m_clone != nullptr && m_clone->code == 777, "fallback create() copies field");
    delete m_clone;

    // 3. Deep vs Shallow vs Reference recursion policies on nested parent->child
    leaf_item src_child("src_child");
    src_child.id = 100;
    parent_item p_src("p_src");
    p_src.tag = 55;
    p_src.child = &src_child;

    // UVM_DEEP
    uvm_copier deep_c("deep_c");
    deep_c.set_recursion_policy(UVM_DEEP);
    parent_item p_deep("p_deep");
    p_deep.copy(p_src, &deep_c);
    check(p_deep.tag == 55, "UVM_DEEP copies top-level scalar");
    check(p_deep.child != nullptr && p_deep.child != &src_child,
          "UVM_DEEP clones nested child");
    check(p_deep.child != nullptr && p_deep.child->id == 100,
          "UVM_DEEP copies nested child fields");
    check(p_deep.active_copier_in_do_copy == &deep_c,
          "custom copier visible in do_execute_op");
    check(p_deep.active_obj_in_do_copy == &p_deep,
          "active object is lhs during do_copy");
    check(p_deep.active_depth_in_do_copy == 1,
          "active depth is 1 during top-level do_copy");
    check(deep_c.get_active_object() == nullptr && deep_c.get_active_object_depth() == 0,
          "active stack empty after copy completes");
    src_child.id = 999;
    check(p_deep.child != nullptr && p_deep.child->id == 100,
          "UVM_DEEP clone isolated from subsequent source mutation");
    src_child.id = 100;

    // UVM_SHALLOW
    uvm_copier shallow_c("shallow_c");
    shallow_c.set_recursion_policy(UVM_SHALLOW);
    parent_item p_shallow("p_shallow");
    p_shallow.copy(p_src, &shallow_c);
    check(p_shallow.tag == 55, "UVM_SHALLOW copies top-level scalar");
    check(p_shallow.child == &src_child, "UVM_SHALLOW copies nested child handle shallowly");

    // UVM_REFERENCE
    uvm_copier ref_c("ref_c");
    ref_c.set_recursion_policy(UVM_REFERENCE);
    leaf_item* ref_ptr = nullptr;
    ref_c.copy_object(ref_ptr, &src_child);
    check(ref_ptr == &src_child, "UVM_REFERENCE copy_object assigns handle directly");
    parent_item p_ref("p_ref");
    p_ref.copy(p_src, &ref_c);
    check(p_ref.child == &src_child, "UVM_REFERENCE preserves child handle");

    // 4. Cycles: 2-node (a -> b -> a) and self-cycle (s -> s)
    node_item a("a"), b("b");
    a.val = 10;
    b.val = 20;
    a.next = &b;
    b.next = &a;

    uvm_copier cyc_c("cyc_c");
    cyc_c.set_recursion_policy(UVM_DEEP);
    node_item* cyc_a = nullptr;
    cyc_c.copy_object(cyc_a, &a);
    check(cyc_a != nullptr && cyc_a != &a, "2-node cycle allocates cyc_a");
    check(cyc_a != nullptr && cyc_a->val == 10, "cyc_a->val == 10");
    check(cyc_a != nullptr && cyc_a->next != nullptr && cyc_a->next != &b,
          "2-node cycle allocates cyc_b");
    check(cyc_a != nullptr && cyc_a->next != nullptr && cyc_a->next->val == 20,
          "cyc_b->val == 20");
    check(cyc_a != nullptr && cyc_a->next != nullptr && cyc_a->next->next == cyc_a,
          "2-node cycle preserves back-edge cyc_b->next == cyc_a");
    if (cyc_a != nullptr)
    {
      delete cyc_a->next;
      delete cyc_a;
    }

    node_item s("s");
    s.val = 30;
    s.next = &s;
    cyc_c.flush();
    node_item* cyc_s = nullptr;
    cyc_c.copy_object(cyc_s, &s);
    check(cyc_s != nullptr && cyc_s != &s, "self-cycle allocates cyc_s");
    check(cyc_s != nullptr && cyc_s->val == 30, "cyc_s->val == 30");
    check(cyc_s != nullptr && cyc_s->next == cyc_s, "self-cycle preserves cyc_s->next == cyc_s");
    delete cyc_s;

    // 5. Diamond DAG sharing: root -> {left, right} -> shared
    leaf_item shared("shared");
    shared.id = 888;
    dag_branch left("left"), right("right");
    left.leaf = &shared;
    right.leaf = &shared;
    dag_root root("root");
    root.left = &left;
    root.right = &right;

    uvm_copier dag_c("dag_c");
    dag_c.set_recursion_policy(UVM_DEEP);
    dag_root* d_root = nullptr;
    dag_c.copy_object(d_root, &root);
    check(d_root != nullptr && d_root != &root, "DAG root cloned");
    check(d_root != nullptr && d_root->left != nullptr && d_root->right != nullptr,
          "DAG branches cloned");
    check(d_root != nullptr && d_root->left->leaf != nullptr &&
          d_root->left->leaf != &shared, "DAG leaf cloned");
    check(d_root != nullptr && d_root->left->leaf == d_root->right->leaf,
          "DAG diamond shares single cloned leaf across both branches");
    check(d_root != nullptr && d_root->left->leaf->id == 888, "DAG leaf value copied");
    check(dag_c.get_num_copies(&shared) == 1, "DAG shared leaf copied exactly once");
    if (d_root != nullptr)
    {
      delete d_root->left->leaf;
      delete d_root->left;
      delete d_root->right;
      delete d_root;
    }

    // 6. object_copied, get_first_copy, get_next_copy, get_last_copy, flush()
    uvm_copier trk_c("trk_c");
    leaf_item d1("d1"), d2("d2"), unrelated("unrelated");
    check(trk_c.object_copied(&d1, &l_src) == 0, "object_copied is 0 before copy");
    check(trk_c.get_num_copies(&l_src) == 0, "get_num_copies is 0 before copy");

    d1.copy(l_src, &trk_c);
    d2.copy(l_src, &trk_c);
    check(trk_c.get_num_copies(&l_src) == 2, "get_num_copies is 2 after two copies");
    check(trk_c.object_copied(&d1, &l_src) != 0, "object_copied(d1, l_src) != 0");
    check(trk_c.object_copied(&d2, &l_src) != 0, "object_copied(d2, l_src) != 0");
    check(trk_c.object_copied(&unrelated, &l_src) == 0, "object_copied(unrelated, l_src) == 0");

    uvm_object* iter = nullptr;
    check(trk_c.get_first_copy(&l_src, iter) && iter == &d1, "get_first_copy == d1");
    check(trk_c.get_next_copy(&l_src, iter) && iter == &d2, "get_next_copy == d2");
    check(!trk_c.get_next_copy(&l_src, iter), "get_next_copy at end returns false");
    check(trk_c.get_last_copy(&l_src, iter) && iter == &d2, "get_last_copy == d2");

    trk_c.flush();
    check(trk_c.get_num_copies(&l_src) == 0, "flush clears get_num_copies");
    check(trk_c.object_copied(&d1, &l_src) == 0, "flush clears object_copied");
    check(!trk_c.get_first_copy(&l_src, iter), "flush clears get_first_copy");
    check(!trk_c.get_last_copy(&l_src, iter), "flush clears get_last_copy");

    // 7. Legacy 1-arg copy + recursive copy inside do_copy without premature flush
    composite_item comp_src("comp_src"), comp_dst("comp_dst");
    comp_src.outer_val = 123;
    comp_src.embedded.id = 456;
    comp_dst.copy(comp_src);
    check(comp_dst.outer_val == 123, "legacy 1-arg copy copies outer_val");
    check(comp_dst.embedded.id == 456, "recursive 1-arg copy copies embedded.id");
    check(comp_dst.active_present_before_inner, "outer active object present before inner copy");
    check(comp_dst.active_present_after_inner,
          "outer active object preserved after inner copy (no premature flush)");
    check(uvm_copier::get_default()->get_active_object() == nullptr,
          "default copier active stack clean after top-level copy");
    check(uvm_copier::get_default()->get_num_copies(&comp_src) == 0,
          "default copier flushed after top-level copy");

    // Self-copy no-op
    comp_dst.copy(comp_dst, &trk_c);
    check(comp_dst.outer_val == 123, "self copy(x, &c) is no-op");

    // 8. Exception safety (1-arg default copier & 2-arg explicit copier)
    // (a) Heap-allocated target throws in 1-arg copy(), gets deleted, then default copy_object(fresh, &s)
    {
      throwing_leaf_item s("s");
      s.val = 321;
      throwing_leaf_item* doomed = new throwing_leaf_item("doomed");
      doomed->throw_in_do_copy = true;
      bool caught = false;
      try
      {
        doomed->copy(s);
      }
      catch (const std::runtime_error&)
      {
        caught = true;
      }
      delete doomed;
      check(caught, "1-arg copy() propagated exception from do_copy()");
      check(uvm_copier::get_default()->get_active_object() == nullptr,
            "default copier active stack empty after exception");
      check(uvm_copier::get_default()->get_num_copies(&s) == 0,
            "default copier flushed after exception in 1-arg copy()");

      throwing_leaf_item* fresh = nullptr;
      uvm_copier::get_default()->copy_object(fresh, &s);
      check(fresh != nullptr && fresh->val == 321,
            "default copy_object(fresh, &s) after doomed delete allocates fresh valid copy");
      uvm_copier::get_default()->flush();
      delete fresh;
    }

    // (b) Explicit uvm_copier nested child throws; verify rollback & prior mapping preservation & retry
    {
      throwing_parent_item p_src2("p_src2");
      p_src2.pval = 55;
      p_src2.child.val = 77;
      throwing_parent_item p_dst2("p_dst2");
      p_dst2.child.throw_in_do_copy = true;

      uvm_copier exc_c("exc_c");
      bool caught = false;
      try
      {
        p_dst2.copy(p_src2, &exc_c);
      }
      catch (const std::runtime_error&)
      {
        caught = true;
      }
      check(caught, "2-arg nested copy() propagated child exception");
      check(exc_c.get_active_object() == nullptr && exc_c.get_active_object_depth() == 0,
            "explicit copier active stack empty after nested exception");
      check(exc_c.get_num_copies(&p_src2) == 0,
            "explicit copier rolled back parent copy count after nested exception");
      check(exc_c.get_num_copies(&p_src2.child) == 0,
            "explicit copier rolled back child copy count after nested exception");
      check(exc_c.object_copied(&p_dst2, &p_src2) == uvm_copier::CREATED,
            "parent copy state rolled back to CREATED");
      check(exc_c.object_copied(&p_dst2.child, &p_src2.child) == uvm_copier::CREATED,
            "child copy state rolled back to CREATED");

      // Retry after clearing throw flag
      p_dst2.child.throw_in_do_copy = false;
      p_dst2.copy(p_src2, &exc_c);
      check(p_dst2.pval == 55 && p_dst2.child.val == 77,
            "retry parent_dst.copy(parent_src, &exc_c) succeeds after rollback");
      check(exc_c.object_copied(&p_dst2, &p_src2) == uvm_copier::FINISHED &&
            exc_c.object_copied(&p_dst2.child, &p_src2.child) == uvm_copier::FINISHED,
            "retry copy states reach FINISHED");

      // Prior copy map entry preserved if subsequent copy throws in do_execute_op
      exc_c.flush();
      throwing_leaf_item good_dst("good_dst"), bad_dst("bad_dst");
      good_dst.copy(p_src2.child, &exc_c);
      bad_dst.throw_in_do_execute_op = true;
      bool op_caught = false;
      try
      {
        bad_dst.copy(p_src2.child, &exc_c);
      }
      catch (const std::runtime_error&)
      {
        op_caught = true;
      }
      check(op_caught, "do_execute_op() exception propagated");
      check(exc_c.get_num_copies(&p_src2.child) == 1,
            "prior copy count preserved when second copy throws in do_execute_op");
      check(exc_c.object_copied(&good_dst, &p_src2.child) == uvm_copier::FINISHED,
            "prior copy remains FINISHED");
      check(exc_c.object_copied(&bad_dst, &p_src2.child) == uvm_copier::CREATED,
            "failed copy rolled back to CREATED");
      throwing_leaf_item* mapped = nullptr;
      exc_c.copy_object(mapped, &p_src2.child);
      check(mapped == &good_dst,
            "copy_map restored to prior good_dst after second copy threw");

      // Null-lhs allocation that throws inside do_copy must not leak or assign lhs
      throwing_leaf_item poison_src("poison_src");
      poison_src.val = -999;
      throwing_leaf_item* null_lhs = nullptr;
      bool alloc_caught = false;
      try
      {
        exc_c.copy_object(null_lhs, &poison_src);
      }
      catch (const std::runtime_error&)
      {
        alloc_caught = true;
      }
      check(alloc_caught, "null-lhs copy_object propagated exception");
      check(null_lhs == nullptr, "null-lhs remains nullptr after exception");
      check(exc_c.get_num_copies(&poison_src) == 0,
            "null-lhs copy_object rolled back after exception");
    }

    // (c) Default copier retry after exception succeeds
    {
      throwing_leaf_item def_src("def_src"), def_dst("def_dst");
      def_src.val = 456;
      def_dst.throw_in_do_copy = true;
      bool caught = false;
      try
      {
        def_dst.copy(def_src);
      }
      catch (const std::runtime_error&)
      {
        caught = true;
      }
      check(caught, "1-arg copy() exception caught before retry");
      def_dst.throw_in_do_copy = false;
      def_dst.copy(def_src);
      check(def_dst.val == 456, "default copier retry after exception copies value");
      check(uvm_copier::get_default()->get_num_copies(&def_src) == 0,
            "default copier flushed after retry");
    }

    // 9. copy_object(nullptr, rhs) respects factory type override
    {
      uvm_factory::get()->set_type_override_by_type(
        ovr_base_item::get_type(), ovr_derived_item::get_type());
      ovr_base_item ovr_src("ovr_src");
      ovr_src.base_val = 654;
      uvm_copier ovr_c("ovr_c");
      ovr_base_item* ovr_dst = nullptr;
      ovr_c.copy_object(ovr_dst, &ovr_src);
      ovr_derived_item* ovr_drv = dynamic_cast<ovr_derived_item*>(ovr_dst);
      check(ovr_dst != nullptr && ovr_dst->base_val == 654,
            "factory-overridden copy_object copies base field");
      check(ovr_drv != nullptr && ovr_drv->derived_tag == 777,
            "copy_object(nullptr, rhs) respects set_type_override_by_type");
      delete ovr_dst;
    }

    std::cout << "copier_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
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
  run_test("copier_test");
  return 0;
}
