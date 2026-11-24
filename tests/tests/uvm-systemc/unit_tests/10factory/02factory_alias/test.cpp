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

// Unit test for IEEE 1800.2-2020 §8.2.4 uvm_factory::set_type_alias() and
// uvm_factory::set_inst_alias() (wrapper and string overloads, instance
// precedence, wildcards, alias chaining, deferred resolution, overrides,
// introspection, debug_create_by_name, and factory->print()).

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class pkt_base : public uvm_object
{
 public:
  explicit pkt_base( const std::string& name = "pkt_base" ) : uvm_object(name) {}
  virtual std::string tag() const { return "pkt_base"; }
  UVM_OBJECT_UTILS(pkt_base);
};

class pkt_a : public pkt_base
{
 public:
  explicit pkt_a( const std::string& name = "pkt_a" ) : pkt_base(name) {}
  virtual std::string tag() const { return "pkt_a"; }
  UVM_OBJECT_UTILS(pkt_a);
};

class pkt_b : public pkt_base
{
 public:
  explicit pkt_b( const std::string& name = "pkt_b" ) : pkt_base(name) {}
  virtual std::string tag() const { return "pkt_b"; }
  UVM_OBJECT_UTILS(pkt_b);
};

class pkt_orig_a : public pkt_base
{
 public:
  explicit pkt_orig_a( const std::string& name = "pkt_orig_a" ) : pkt_base(name) {}
  virtual std::string tag() const { return "pkt_orig_a"; }
  UVM_OBJECT_UTILS(pkt_orig_a);
};

class pkt_ovr_a : public pkt_orig_a
{
 public:
  explicit pkt_ovr_a( const std::string& name = "pkt_ovr_a" ) : pkt_orig_a(name) {}
  virtual std::string tag() const { return "pkt_ovr_a"; }
  UVM_OBJECT_UTILS(pkt_ovr_a);
};

class pkt_orig_b : public pkt_base
{
 public:
  explicit pkt_orig_b( const std::string& name = "pkt_orig_b" ) : pkt_base(name) {}
  virtual std::string tag() const { return "pkt_orig_b"; }
  UVM_OBJECT_UTILS(pkt_orig_b);
};

class pkt_ovr_b : public pkt_orig_b
{
 public:
  explicit pkt_ovr_b( const std::string& name = "pkt_ovr_b" ) : pkt_orig_b(name) {}
  virtual std::string tag() const { return "pkt_ovr_b"; }
  UVM_OBJECT_UTILS(pkt_ovr_b);
};

template <int W>
class param_pkt : public pkt_base
{
 public:
  explicit param_pkt( const std::string& name = "param_pkt" ) : pkt_base(name) {}
  virtual std::string tag() const { return "param_pkt_8"; }
  UVM_OBJECT_PARAM_UTILS(param_pkt<W>);
};

class anon_pkt : public pkt_base
{
 public:
  explicit anon_pkt( const std::string& name = "anon_pkt" ) : pkt_base(name) {}
  virtual std::string tag() const { return "anon_pkt"; }
};

class anon_wrapper : public uvm_object_wrapper
{
 public:
  virtual uvm_object* create_object( const std::string& name = "" )
  {
    return new anon_pkt(name.empty() ? "anon_pkt" : name);
  }
  virtual const std::string get_type_name() const { return "<unknown>"; }
};

class drv_base : public uvm_component
{
 public:
  explicit drv_base( uvm_component_name name ) : uvm_component(name) {}
  virtual std::string kind_tag() const { return "drv_base"; }
  UVM_COMPONENT_UTILS(drv_base);
};

class drv_a : public drv_base
{
 public:
  explicit drv_a( uvm_component_name name ) : drv_base(name) {}
  virtual std::string kind_tag() const { return "drv_a"; }
  UVM_COMPONENT_UTILS(drv_a);
};

class drv_b : public drv_base
{
 public:
  explicit drv_b( uvm_component_name name ) : drv_base(name) {}
  virtual std::string kind_tag() const { return "drv_b"; }
  UVM_COMPONENT_UTILS(drv_b);
};

class drv_a_ovr : public drv_a
{
 public:
  explicit drv_a_ovr( uvm_component_name name ) : drv_a(name) {}
  virtual std::string kind_tag() const { return "drv_a_ovr"; }
  UVM_COMPONENT_UTILS(drv_a_ovr);
};

class drv_c : public drv_base
{
 public:
  explicit drv_c( uvm_component_name name ) : drv_base(name) {}
  virtual std::string kind_tag() const { return "drv_c"; }
  UVM_COMPONENT_UTILS(drv_c);
};

class drv_c_ovr : public drv_c
{
 public:
  explicit drv_c_ovr( uvm_component_name name ) : drv_c(name) {}
  virtual std::string kind_tag() const { return "drv_c_ovr"; }
  UVM_COMPONENT_UTILS(drv_c_ovr);
};

// Custom wrapper registered dynamically after alias setup to test deferred resolution
class delayed_obj : public pkt_base
{
 public:
  explicit delayed_obj( const std::string& name = "delayed_obj" ) : pkt_base(name) {}
  virtual std::string tag() const { return "delayed_obj"; }
};

class delayed_wrapper : public uvm_object_wrapper
{
 public:
  virtual uvm_object* create_object( const std::string& name = "" )
  {
    return new delayed_obj(name.empty() ? "delayed_obj" : name);
  }
  virtual const std::string get_type_name() const { return "delayed_obj_type"; }
};

class late_b_wrapper : public uvm_object_wrapper
{
 public:
  virtual uvm_object* create_object( const std::string& name = "" )
  {
    return new pkt_base(name.empty() ? "late_type_b" : name);
  }
  virtual const std::string get_type_name() const { return "late_type_b"; }
};

class late_c_wrapper : public uvm_object_wrapper
{
 public:
  virtual uvm_object* create_object( const std::string& name = "" )
  {
    return new pkt_base(name.empty() ? "late_type_c" : name);
  }
  virtual const std::string get_type_name() const { return "late_type_c"; }
};

class late_inst_comp_wrapper : public uvm_object_wrapper
{
 public:
  virtual uvm_component* create_component( const std::string& name = "",
                                           uvm_component* parent = nullptr )
  {
    (void)parent;
    return new drv_base(name.empty() ? "late_inst_comp" : name.c_str());
  }
  virtual const std::string get_type_name() const { return "late_inst_comp"; }
};

class abs_h_drv : public drv_base
{
 public:
  explicit abs_h_drv( uvm_component_name name ) : drv_base(name) {}
  virtual std::string kind_tag() const = 0;
  UVM_COMPONENT_ABSTRACT_UTILS(abs_h_drv);
};

class abs_h_conc : public abs_h_drv
{
 public:
  explicit abs_h_conc( uvm_component_name name ) : abs_h_drv(name) {}
  virtual std::string kind_tag() const { return "abs_h_conc"; }
  UVM_COMPONENT_UTILS(abs_h_conc);
};

class drv_h_x : public drv_base
{
 public:
  explicit drv_h_x( uvm_component_name name ) : drv_base(name) {}
  virtual std::string kind_tag() const { return "drv_h_x"; }
  UVM_COMPONENT_UTILS(drv_h_x);
};

class drv_h_y : public drv_h_x
{
 public:
  explicit drv_h_y( uvm_component_name name ) : drv_h_x(name) {}
  virtual std::string kind_tag() const { return "drv_h_y"; }
  UVM_COMPONENT_UTILS(drv_h_y);
};

class drv_h_w : public drv_base
{
 public:
  explicit drv_h_w( uvm_component_name name ) : drv_base(name) {}
  virtual std::string kind_tag() const { return "drv_h_w"; }
  UVM_COMPONENT_UTILS(drv_h_w);
};

class drv_h_u : public drv_base
{
 public:
  explicit drv_h_u( uvm_component_name name ) : drv_base(name) {}
  virtual std::string kind_tag() const { return "drv_h_u"; }
  UVM_COMPONENT_UTILS(drv_h_u);
};

class late_h3_wrapper : public uvm_object_wrapper
{
 public:
  virtual uvm_component* create_component( const std::string& name = "",
                                           uvm_component* parent = nullptr )
  {
    (void)parent;
    return new drv_base(name.empty() ? "late_h3_target" : name.c_str());
  }
  virtual const std::string get_type_name() const { return "late_h3_target"; }
};

class late_mhop_wrapper : public uvm_object_wrapper
{
 public:
  virtual uvm_component* create_component( const std::string& name = "",
                                           uvm_component* parent = nullptr )
  {
    (void)parent;
    return new drv_base(name.empty() ? "late_mhop_target" : name.c_str());
  }
  virtual const std::string get_type_name() const { return "late_mhop_target"; }
};

class alias_report_catcher : public uvm_report_catcher
{
 public:
  int alsrgd_count;
  int typdup_count;

  alias_report_catcher() : alsrgd_count(0), typdup_count(0) {}

  virtual action_e do_catch()
  {
    if (get_id() == "ALSRGD" && get_severity() == UVM_WARNING)
    {
      alsrgd_count++;
      return CAUGHT;
    }
    if (get_id() == "TYPDUP" && get_severity() == UVM_WARNING)
    {
      typdup_count++;
      return CAUGHT;
    }
    return THROW;
  }
};

class agent_stub : public uvm_component
{
 public:
  std::string alias_name;
  std::string expected_tag;
  bool ok;

  explicit agent_stub( uvm_component_name name )
  : uvm_component(name), alias_name("drv_inst_alias"), expected_tag("drv_a"), ok(false) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_component::build_phase(phase);
    uvm_factory* factory = uvm_factory::get();
    uvm_component* c = factory->create_component_by_name(alias_name, get_full_name(), "drv", this);
    drv_base* d = dynamic_cast<drv_base*>(c);
    ok = (d != nullptr && d->kind_tag() == expected_tag);
  }

  UVM_COMPONENT_UTILS(agent_stub);
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  bool all_ok;
  delayed_wrapper* dyn_wrapper;
  anon_wrapper* unk_wrapper;
  agent_stub* ag1;
  agent_stub* ag2;

  explicit test( uvm_component_name name )
  : uvm_test(name), all_ok(true), dyn_wrapper(nullptr), unk_wrapper(nullptr),
    ag1(nullptr), ag2(nullptr) {}

  void check( bool cond, const std::string& label )
  {
    if (!cond)
    {
      all_ok = false;
      std::cout << "FAIL: " << label << std::endl;
    }
  }

  std::string obj_tag( uvm_object* o )
  {
    pkt_base* p = dynamic_cast<pkt_base*>(o);
    return (p != nullptr) ? p->tag() : std::string("null");
  }

  std::string comp_tag( uvm_component* c )
  {
    drv_base* d = dynamic_cast<drv_base*>(c);
    return (d != nullptr) ? d->kind_tag() : std::string("null");
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    uvm_factory* factory = uvm_factory::get();

    // 1. Wrapper-based type aliases (including parameterized object and <unknown> wrapper)
    factory->set_type_alias("pkt_alias", pkt_a::get_type());
    factory->set_type_alias("param8_alias", param_pkt<8>::get_type());
    unk_wrapper = new anon_wrapper();
    factory->set_type_alias("anon_alias", unk_wrapper);
    factory->set_type_alias("drv_type_alias", drv_a::get_type());

    // 2. String-name type alias & alias-to-alias chaining
    factory->set_type_alias("pkt_str_alias", "pkt_b");
    factory->set_type_alias("pkt_chain_1", "pkt_str_alias");
    factory->set_type_alias("pkt_chain_2", "pkt_chain_1");

    // 3. Cycle guard check (must not loop)
    factory->set_type_alias("cyc_a", "cyc_b");
    factory->set_type_alias("cyc_b", "cyc_a");
    check(!factory->is_type_name_registered("cyc_a"), "cyclic alias not registered");

    // 4. Deferred alias before target registration
    factory->set_type_alias("delayed_alias", "delayed_obj_type");
    check(!factory->is_type_name_registered("delayed_alias"), "deferred alias not ready yet");
    dyn_wrapper = new delayed_wrapper();
    factory->do_register(dyn_wrapper);
    check(factory->is_type_name_registered("delayed_alias"), "deferred alias resolved on do_register");
    check(factory->find_wrapper_by_name("delayed_alias") == dyn_wrapper,
          "find_wrapper_by_name(delayed_alias) == dyn_wrapper");

    // 5. Instance aliases (exact path + wildcards '*' and '?')
    factory->set_inst_alias("pkt_alias", pkt_b::get_type(), "test.special.*");
    factory->set_inst_alias("pkt_alias", "pkt_orig_a", "test.qmark_?");
    factory->set_inst_alias("drv_inst_alias", drv_b::get_type(), "test.drv_exact");
    factory->set_inst_alias("drv_inst_alias", "drv_a", "test.ag?.drv");

    // 6. Introspection checks
    check(factory->is_type_name_registered("pkt_alias"), "is_type_name_registered(pkt_alias)");
    check(factory->is_type_name_registered("pkt_chain_2"), "is_type_name_registered(pkt_chain_2)");
    check(factory->find_wrapper_by_name("pkt_alias") == pkt_a::get_type(),
          "find_wrapper_by_name(pkt_alias) == pkt_a");
    check(factory->find_wrapper_by_name("pkt_chain_2") == pkt_b::get_type(),
          "find_wrapper_by_name(pkt_chain_2) == pkt_b");
    check(factory->find_wrapper_by_name("param8_alias") == param_pkt<8>::get_type(),
          "find_wrapper_by_name(param8_alias) == param_pkt<8>");
    check(factory->find_wrapper_by_name("anon_alias") == unk_wrapper,
          "find_wrapper_by_name(anon_alias) == unk_wrapper");

    // 7. Create objects via type alias, chain alias, deferred alias, parameterized alias, and inst alias precedence
    uvm_object* o_type = factory->create_object_by_name("pkt_alias", "test.normal", "o1");
    uvm_object* o_inst_wild = factory->create_object_by_name("pkt_alias", "test.special", "o2");
    uvm_object* o_inst_q = factory->create_object_by_name("pkt_alias", "test", "qmark_x");
    uvm_object* o_chain = factory->create_object_by_name("pkt_chain_2", "", "o3");
    uvm_object* o_delayed = factory->create_object_by_name("delayed_alias", "", "o4");
    uvm_object* o_p8 = factory->create_object_by_name("param8_alias", "", "p8");
    uvm_object* o_anon = factory->create_object_by_name("anon_alias", "", "pa");

    check(obj_tag(o_type) == "pkt_a", "pkt_alias normal -> pkt_a");
    check(obj_tag(o_inst_wild) == "pkt_b", "pkt_alias special.* -> pkt_b (inst alias precedence)");
    check(obj_tag(o_inst_q) == "pkt_orig_a", "pkt_alias qmark_? -> pkt_orig_a");
    check(obj_tag(o_chain) == "pkt_b", "pkt_chain_2 -> pkt_b");
    check(obj_tag(o_delayed) == "delayed_obj", "delayed_alias -> delayed_obj");
    check(obj_tag(o_p8) == "param_pkt_8", "param8_alias -> param_pkt<8>");
    check(obj_tag(o_anon) == "anon_pkt", "anon_alias -> anon_pkt");

    // 8. Bidirectional alias + override interactions (objects)
    // 8a: Override registered on original -> created via alias
    factory->set_type_alias("alias_orig_a", pkt_orig_a::get_type());
    factory->set_type_override_by_type(pkt_orig_a::get_type(), pkt_ovr_a::get_type());
    uvm_object* o_bidir1 = factory->create_object_by_name("alias_orig_a", "", "bidir1");
    check(obj_tag(o_bidir1) == "pkt_ovr_a", "override on original -> create by alias");

    // 8b: Override registered on alias -> created via type_id::create()
    factory->set_type_alias("alias_orig_b", pkt_orig_b::get_type());
    factory->set_type_override_by_name("alias_orig_b", "pkt_ovr_b");
    pkt_orig_b* o_bidir2 = pkt_orig_b::type_id::create("bidir2");
    check(o_bidir2 != nullptr && o_bidir2->tag() == "pkt_ovr_b",
          "override on alias -> create by type_id::create()");

    // 9. Components: type alias, inst aliases, and alias + inst override
    ag1 = agent_stub::type_id::create("ag1", this);
    ag1->alias_name = "drv_inst_alias";
    ag1->expected_tag = "drv_a";

    ag2 = agent_stub::type_id::create("ag2", this);
    ag2->alias_name = "drv_ovr_alias";
    ag2->expected_tag = "drv_a_ovr";

    uvm_component* d_type = factory->create_component_by_name("drv_type_alias", get_full_name(), "d_type", this);
    uvm_component* d_exact = factory->create_component_by_name("drv_inst_alias", get_full_name(), "drv_exact", this);
    check(comp_tag(d_type) == "drv_a", "drv_type_alias -> drv_a");
    check(comp_tag(d_exact) == "drv_b", "drv_inst_alias exact -> drv_b");

    // Inst override on original + inst alias (exercised by ag2->build_phase as test.ag2.drv)
    factory->set_inst_alias("drv_ovr_alias", drv_a::get_type(), "test.ag2.*");
    factory->set_inst_override_by_type(drv_a::get_type(), drv_a_ovr::get_type(), "test.ag2.*");

    // Inst override on alias -> type_id::create()
    factory->set_type_alias("drv_c_alias", drv_c::get_type());
    factory->set_inst_override_by_name("drv_c_alias", "drv_c_ovr", "test.c_ovr");
    drv_c* d_c_ovr = drv_c::type_id::create("c_ovr", this);
    check(d_c_ovr != nullptr && d_c_ovr->kind_tag() == "drv_c_ovr",
          "inst override on alias -> type_id::create()");

    // 10. Regression checks for deferred overrides, mixed inst+type alias overrides,
    // deduplicated replace=false overrides, colliding ALSRGD, and edge cases
    alias_report_catcher als_catcher;
    uvm_report_cb::add(nullptr, &als_catcher);

    // 10a (Bug 1): deferred set_type_override_by_name on target before do_register,
    // then create via alias
    factory->set_type_override_by_name("late_type_b", "pkt_ovr_a");
    factory->set_type_alias("early_b", "late_type_b");
    late_b_wrapper* late_b_w = new late_b_wrapper();
    factory->do_register(late_b_w);
    uvm_object* o_b_alias = factory->create_object_by_name("early_b", "", "b_alias");
    uvm_object* o_b_orig = factory->create_object_by_name("late_type_b", "", "b_orig");
    uvm_object* o_b_type = factory->create_object_by_type(late_b_w, "", "b_type");
    check(obj_tag(o_b_alias) == "pkt_ovr_a", "Bug1: deferred type_override_by_name via alias");
    check(obj_tag(o_b_orig) == "pkt_ovr_a", "Bug1: deferred type_override_by_name via orig");
    check(obj_tag(o_b_type) == "pkt_ovr_a", "Bug1: deferred type_override_by_name via type");

    // 10b (Bug 2): set_inst_alias + set_type_alias + set_inst_override_by_name
    // Exact path match
    factory->set_inst_alias("drv_mix", drv_a::get_type(), "test.mix_ag1.*");
    factory->set_type_alias("drv_mix", drv_c::get_type());
    factory->set_inst_override_by_name("drv_mix", "drv_a_ovr", "test.mix_ag1.*");
    uvm_component* d_mix1 = factory->create_component_by_name("drv_mix", "test.mix_ag1", "mix1_d1", this);
    uvm_component* d_mix2 = factory->create_component_by_name("drv_mix", "test.mix_ag2", "mix1_d2", this);
    check(comp_tag(d_mix1) == "drv_a_ovr", "Bug2a: inst_alias + type_alias + inst_override @ ag1 -> drv_a_ovr");
    check(comp_tag(d_mix2) == "drv_c", "Bug2a: inst_alias + type_alias + inst_override @ ag2 -> drv_c");

    // Narrower inst_override under wider inst_alias
    factory->set_inst_alias("drv_mix2", drv_a::get_type(), "test.mix2.*");
    factory->set_type_alias("drv_mix2", drv_c::get_type());
    factory->set_inst_override_by_name("drv_mix2", "drv_a_ovr", "test.mix2.sub.*");
    uvm_component* d_mix2_sub = factory->create_component_by_name("drv_mix2", "test.mix2.sub", "mix2_d1", this);
    uvm_component* d_mix2_oth = factory->create_component_by_name("drv_mix2", "test.mix2.other", "mix2_d2", this);
    uvm_component* d_mix2_out = factory->create_component_by_name("drv_mix2", "test.other", "mix2_d3", this);
    check(comp_tag(d_mix2_sub) == "drv_a_ovr", "Bug2b: narrower inst_override under wider inst_alias");
    check(comp_tag(d_mix2_oth) == "drv_a", "Bug2b: sibling under wider inst_alias stays drv_a");
    check(comp_tag(d_mix2_out) == "drv_c", "Bug2b: outside inst_alias falls back to type_alias drv_c");

    // Wider inst_override covering both inst_alias and type_alias
    factory->set_inst_alias("drv_mix3", drv_a::get_type(), "test.mix3.ag1.*");
    factory->set_type_alias("drv_mix3", drv_c::get_type());
    factory->set_inst_override_by_name("drv_mix3", "drv_b", "test.mix3.*");
    uvm_component* d_mix3_1 = factory->create_component_by_name("drv_mix3", "test.mix3.ag1", "mix3_d1", this);
    uvm_component* d_mix3_2 = factory->create_component_by_name("drv_mix3", "test.mix3.ag2", "mix3_d2", this);
    check(comp_tag(d_mix3_1) == "drv_b", "Bug2c: wider inst_override covers inst_alias");
    check(comp_tag(d_mix3_2) == "drv_b", "Bug2c: wider inst_override covers type_alias");

    // 10c (Bug 3): deferred set_type_alias deduplicates m_type_overrides & respects replace=false
    late_c_wrapper* late_c_w = new late_c_wrapper();
    factory->do_register(late_c_w);
    factory->set_type_override_by_name("early_c", "pkt_ovr_a");
    factory->set_type_override_by_type(late_c_w, pkt_b::get_type(), false);
    factory->set_type_alias("early_c", late_c_w);
    uvm_object* o_c_alias = factory->create_object_by_name("early_c", "", "c_alias");
    uvm_object* o_c_type = factory->create_object_by_type(late_c_w, "", "c_type");
    check(obj_tag(o_c_alias) == "pkt_ovr_a", "Bug3: deferred alias replace=false preserves first override (alias)");
    check(obj_tag(o_c_type) == "pkt_ovr_a", "Bug3: deferred alias replace=false preserves first override (type)");
    factory->set_type_override_by_type(late_c_w, pkt_ovr_b::get_type(), true);
    uvm_object* o_c_repl = factory->create_object_by_name("early_c", "", "c_repl");
    check(obj_tag(o_c_repl) == "pkt_ovr_b", "Bug3: subsequent replace=true updates single entry");

    // 10d (Bug 4): colliding set_type_alias emits ALSRGD and is not stored/printed
    factory->set_type_alias("pkt_a", pkt_b::get_type());
    factory->set_type_alias("pkt_a", "unreg_shadow_target");
    check(als_catcher.alsrgd_count == 2, "Bug4: colliding aliases emit ALSRGD twice");
    check(factory->find_wrapper_by_name("pkt_a") == pkt_a::get_type(),
          "Bug4: pkt_a still resolves to pkt_a");

    // 10e (Reviewer 2 minor): type override on alias does not bleed onto inst_alias target
    factory->set_type_alias("dual_alias", pkt_orig_a::get_type());
    factory->set_inst_alias("dual_alias", pkt_b::get_type(), "test.dual_inst.*");
    factory->set_type_override_by_name("dual_alias", "pkt_ovr_a");
    uvm_object* o_dual_norm = factory->create_object_by_name("dual_alias", "test.dual_normal", "p1");
    uvm_object* o_dual_inst = factory->create_object_by_name("dual_alias", "test.dual_inst", "p2");
    check(obj_tag(o_dual_norm) == "pkt_ovr_a", "Rev2: dual_alias normal uses type_alias override");
    check(obj_tag(o_dual_inst) == "pkt_b", "Rev2: dual_alias inst_alias ignores type_alias override");

    // 10f (Edge cases): wildcard inst_override matching alias, inst_alias->inst_alias chain, "*" on empty path
    factory->set_type_alias("legacy_pkt", pkt_a::get_type());
    factory->set_inst_override_by_name("legacy_*", "pkt_b", "test.leg.*");
    factory->set_type_alias("legacy_late", pkt_orig_b::get_type());
    uvm_object* o_leg1 = factory->create_object_by_name("legacy_pkt", "test.leg", "p1");
    uvm_object* o_leg2 = factory->create_object_by_name("legacy_late", "test.leg", "p2");
    check(obj_tag(o_leg1) == "pkt_b", "Wildcard inst_override matches existing type_alias");
    check(obj_tag(o_leg2) == "pkt_b", "Wildcard inst_override matches deferred type_alias");

    factory->set_inst_alias("inst_base", drv_b::get_type(), "test.ichain.*");
    factory->set_inst_alias("inst_hop", "inst_base", "test.ichain.sub.*");
    uvm_component* d_ichain = factory->create_component_by_name("inst_hop", "test.ichain.sub", "ichain_d1", this);
    check(comp_tag(d_ichain) == "drv_b", "Chained inst_alias -> inst_alias resolves");

    factory->set_inst_alias("star_inst_pkt", pkt_b::get_type(), "*");
    uvm_object* o_star = factory->create_object_by_name("star_inst_pkt", "", "");
    check(obj_tag(o_star) == "pkt_b", "inst_alias with '*' matches empty full_inst_path");

    // 10g (Iteration 3 adversarial regressions: R2-A, R2-B, R2-C, R2-D, R2-E, R2-G2)
    // R2-A: set_type_alias + set_inst_override_by_name before set_inst_alias
    factory->set_type_alias("r2a_drv", drv_c::get_type());
    factory->set_inst_override_by_name("r2a_drv", "drv_b", "test.r2a.*");
    factory->set_inst_alias("r2a_drv", drv_a::get_type(), "test.r2a.ag1.*");
    uvm_component* d_r2a_ag1 = factory->create_component_by_name("r2a_drv", "test.r2a.ag1", "r2a_d1", this);
    uvm_component* d_r2a_ag2 = factory->create_component_by_name("r2a_drv", "test.r2a.ag2", "r2a_d2", this);
    uvm_component* d_r2a_type = factory->create_component_by_type(drv_a::get_type(), "test.r2a.ag1", "r2a_d3", this);
    check(comp_tag(d_r2a_ag1) == "drv_b", "R2-A: type_alias + inst_override before inst_alias applies at ag1");
    check(comp_tag(d_r2a_ag2) == "drv_b", "R2-A: type_alias + inst_override before inst_alias applies at ag2");
    check(comp_tag(d_r2a_type) == "drv_b", "R2-A: propagated inst_override applies via create_component_by_type");

    // R2-B: intersecting non-nested wildcards (*.r2b_ag1.* vs test.r2b.*) with set_type_alias
    factory->set_inst_alias("r2b_drv", drv_a::get_type(), "*.r2b_ag1.*");
    factory->set_type_alias("r2b_drv", drv_c::get_type());
    factory->set_inst_override_by_name("r2b_drv", "drv_b", "test.r2b.*");
    uvm_component* d_r2b_hit = factory->create_component_by_name("r2b_drv", "test.r2b.r2b_ag1", "r2b_d1", this);
    uvm_component* d_r2b_miss = factory->create_component_by_name("r2b_drv", "other.r2b_ag1", "r2b_d2", this);
    check(comp_tag(d_r2b_hit) == "drv_b", "R2-B: intersecting wildcards apply inst_override");
    check(comp_tag(d_r2b_miss) == "drv_a", "R2-B: non-intersecting path keeps inst_alias");

    // R2-C: first-match-wins across m_inst_override_queues vs m_inst_override_name_queues
    factory->set_inst_alias("r2c_drv", drv_a::get_type(), "test.r2c.ag1.*");
    factory->set_inst_alias("r2c_drv", drv_c::get_type(), "test.r2c.ag2.*");
    factory->set_inst_override_by_type(drv_a::get_type(), drv_a_ovr::get_type(), "test.r2c.ag1.*");
    factory->set_inst_override_by_name("r2c_drv", "drv_b", "test.r2c.*");
    uvm_component* d_r2c_ag1 = factory->create_component_by_name("r2c_drv", "test.r2c.ag1", "r2c_d1", this);
    uvm_component* d_r2c_ag2 = factory->create_component_by_name("r2c_drv", "test.r2c.ag2", "r2c_d2", this);
    check(comp_tag(d_r2c_ag1) == "drv_a_ovr", "R2-C: earlier canonical inst_override wins over later wider alias override");
    check(comp_tag(d_r2c_ag2) == "drv_b", "R2-C: later wider alias override applies at non-overlapping ag2");

    // R2-D: first-match-wins between earlier wildcard inst_override and later canonical inst_override after deferred type_alias
    factory->set_inst_override_by_name("leg2_*", "drv_b", "test.leg2.*");
    factory->set_type_alias("leg2_drv", drv_a::get_type());
    factory->set_inst_override_by_type(drv_a::get_type(), drv_a_ovr::get_type(), "test.leg2.sub.*");
    uvm_component* d_r2d_hit = factory->create_component_by_name("leg2_drv", "test.leg2.sub", "r2d_d1", this);
    check(comp_tag(d_r2d_hit) == "drv_b", "R2-D: earlier wildcard inst_override wins over later canonical inst_override");

    // R2-E: self-override via alias emits TYPDUP and preserves existing override
    factory->set_type_alias("alias_pkta_dup", pkt_a::get_type());
    factory->set_type_override_by_type(pkt_a::get_type(), pkt_b::get_type());
    factory->set_type_override_by_name("alias_pkta_dup", "pkt_a");
    uvm_object* o_r2e = factory->create_object_by_name("alias_pkta_dup", "", "r2e_o1");
    check(als_catcher.typdup_count == 1, "R2-E: self-override via alias emits TYPDUP");
    check(obj_tag(o_r2e) == "pkt_b", "R2-E: self-override via alias preserves pkt_a -> pkt_b");
    factory->set_type_override_by_name("late_alias_pkta", "pkt_a");
    factory->set_type_alias("late_alias_pkta", pkt_a::get_type());
    uvm_object* o_r2e_def = factory->create_object_by_name("late_alias_pkta", "", "r2e_o2");
    check(als_catcher.typdup_count == 2, "R2-E: deferred self-override via alias emits TYPDUP");
    check(obj_tag(o_r2e_def) == "pkt_b", "R2-E: deferred self-override via alias preserves pkt_a -> pkt_b");

    // R2-G2: deferred string-based set_inst_alias migrates m_inst_override_name_queues
    factory->set_inst_alias("late_inst_a", "late_inst_comp", "test.late_g2.*");
    factory->set_inst_override_by_name("late_inst_a", "drv_b", "test.late_g2.sub.*");
    late_inst_comp_wrapper* late_inst_w = new late_inst_comp_wrapper();
    factory->do_register(late_inst_w);
    uvm_component* d_r2g_alias = factory->create_component_by_name("late_inst_a", "test.late_g2.sub", "r2g_d1", this);
    uvm_component* d_r2g_type = factory->create_component_by_type(late_inst_w, "test.late_g2.sub", "r2g_d2", this);
    check(comp_tag(d_r2g_alias) == "drv_b", "R2-G2: deferred inst_alias + inst_override via alias");
    check(comp_tag(d_r2g_type) == "drv_b", "R2-G2: deferred inst_alias + inst_override via canonical wrapper");

    uvm_report_cb::do_delete(nullptr, &als_catcher);

    // 10h (Iteration 4 regressions: I3-INTERSECT-TYPE, R3-A, R3-B, R3-C, R3-D, R3-E, R3-F)
    // I3-INTERSECT-TYPE (immediate abstract + type_alias, immediate concrete, deferred H3)
    factory->set_inst_alias("i3_abs_mix", abs_h_drv::get_type(), "*.i3_ag1.*");
    factory->set_type_alias("i3_abs_mix", drv_c::get_type());
    factory->set_inst_override_by_name("i3_abs_mix", "abs_h_conc", "test.i3_sub.*");
    uvm_component* d_i3_name = factory->create_component_by_name("i3_abs_mix", "test.i3_sub.i3_ag1", "i3_c1", this);
    uvm_component* d_i3_type = factory->create_component_by_type(abs_h_drv::get_type(), "test.i3_sub.i3_ag1", "i3_c2", this);
    abs_h_drv* d_i3_tid = abs_h_drv::type_id::create("i3_c3", this, "test.i3_sub.i3_ag1");
    check(comp_tag(d_i3_name) == "abs_h_conc", "I3-INTERSECT-TYPE: abs by_name -> abs_h_conc");
    check(comp_tag(d_i3_type) == "abs_h_conc", "I3-INTERSECT-TYPE: abs by_type -> abs_h_conc");
    check(d_i3_tid != nullptr && d_i3_tid->kind_tag() == "abs_h_conc", "I3-INTERSECT-TYPE: abs type_id::create -> abs_h_conc");

    factory->set_inst_alias("i3_conc", drv_h_x::get_type(), "*.i3_n*");
    factory->set_inst_override_by_name("i3_conc", "drv_h_y", "test.*");
    uvm_component* d_i3_hit = factory->create_component_by_type(drv_h_x::get_type(), "test", "i3_n1", this);
    drv_h_x* d_i3_tid2 = drv_h_x::type_id::create("i3_n2", this);
    uvm_component* d_i3_sib1 = factory->create_component_by_type(drv_h_x::get_type(), "other", "i3_n3", nullptr);
    uvm_component* d_i3_sib2 = factory->create_component_by_type(drv_h_x::get_type(), "test", "i3_other", this);
    check(comp_tag(d_i3_hit) == "drv_h_y", "I3-INTERSECT-TYPE: pure inst_alias by_type -> drv_h_y");
    check(d_i3_tid2 != nullptr && d_i3_tid2->kind_tag() == "drv_h_y", "I3-INTERSECT-TYPE: pure inst_alias type_id::create -> drv_h_y");
    check(comp_tag(d_i3_sib1) == "drv_h_x", "I3-INTERSECT-TYPE: non-matching sibling1 stays drv_h_x");
    check(comp_tag(d_i3_sib2) == "drv_h_x", "I3-INTERSECT-TYPE: non-matching sibling2 stays drv_h_x");

    factory->set_inst_alias("late_h3_alias", "late_h3_target", "test.h3*");
    factory->set_inst_override_by_name("late_h3_alias", "drv_b", "*.h3_leaf*");
    late_h3_wrapper* late_h3_w = new late_h3_wrapper();
    factory->do_register(late_h3_w);
    factory->set_inst_override_by_type(late_h3_w, drv_c::get_type(), "test.h3.h3_leaf_hit");
    uvm_component* d_h3_hit = factory->create_component_by_type(late_h3_w, "test.h3", "h3_leaf_hit", this);
    uvm_component* d_h3_miss1 = factory->create_component_by_type(late_h3_w, "test.h3", "h3_other", this);
    uvm_component* d_h3_miss2 = factory->create_component_by_type(late_h3_w, "other_top", "h3_leaf_miss", this);
    check(comp_tag(d_h3_hit) == "drv_b", "I3-INTERSECT-TYPE H3: earlier deferred inst_alias beats later canonical");
    check(comp_tag(d_h3_miss1) == "drv_base", "I3-INTERSECT-TYPE H3: non-matching leaf stays late_h3_target");
    check(comp_tag(d_h3_miss2) == "drv_base", "I3-INTERSECT-TYPE H3: non-matching prefix stays late_h3_target");

    // R3-A: dead-end inst_alias branch does not pollute req_chain
    factory->set_inst_alias("r3a_al", "r3a_unreg_deadend", "test.r3a.ag1.*");
    factory->set_inst_alias("r3a_al", drv_h_x::get_type(), "test.r3a.*");
    factory->set_inst_override_by_name("r3a_unreg_deadend", "drv_b", "test.r3a.*");
    uvm_component* d_r3a = factory->create_component_by_name("r3a_al", "test.r3a.ag1", "r3a_d1", this);
    check(comp_tag(d_r3a) == "drv_h_x", "R3-A: dead-end inst_alias branch does not leak override");

    // R3-B: multi-candidate debug_create_by_name initializes all selected flags
    factory->set_inst_override_by_name("r3b_*", "drv_b", "test.r3b.*");
    factory->set_inst_alias("r3b_drv", drv_h_x::get_type(), "test.r3b.ag1.*");
    factory->set_inst_override_by_type(drv_h_x::get_type(), drv_h_y::get_type(), "test.r3b.ag1.*");
    factory->debug_create_by_name("r3b_drv", "test.r3b.ag1", "r3b_dbg");

    // R3-C: debug_create_by_name on self-instance-override emits no OVRDLOOP
    factory->set_inst_override_by_type(drv_h_u::get_type(), drv_h_u::get_type(), "test.r3c.*");
    factory->set_type_override_by_type(drv_h_u::get_type(), drv_b::get_type());
    uvm_component* d_r3c_a = factory->create_component_by_name("drv_h_u", "test.r3c", "r3c_d1", this);
    check(comp_tag(d_r3c_a) == "drv_h_u", "R3-C: self-inst-override creates drv_h_u");
    factory->debug_create_by_name("drv_h_u", "test.r3c", "r3c_dbg");

    factory->set_inst_alias("r3c_al", drv_h_w::get_type(), "test.r3c_b.*");
    factory->set_inst_override_by_name("r3c_al", "drv_h_w", "test.r3c_b.*");
    factory->set_type_override_by_type(drv_h_w::get_type(), drv_b::get_type());
    uvm_component* d_r3c_b = factory->create_component_by_name("r3c_al", "test.r3c_b", "r3c_d2", this);
    check(comp_tag(d_r3c_b) == "drv_h_w", "R3-C: alias self-inst-override creates drv_h_w");
    factory->debug_create_by_name("r3c_al", "test.r3c_b", "r3c_dbg2");

    // R3-D: m_propagate_inst_alias_overrides transfers earliest seq onto existing override
    factory->set_type_alias("r3d_al", drv_c::get_type());
    factory->set_inst_override_by_name("r3d_al", "drv_b", "test.r3d.*");
    factory->set_inst_override_by_type(drv_h_x::get_type(), drv_h_w::get_type(), "test.r3d.*");
    factory->set_inst_override_by_type(drv_h_x::get_type(), drv_b::get_type(), "test.r3d.*");
    factory->set_inst_alias("r3d_al", drv_h_x::get_type(), "test.r3d.*");
    uvm_component* d_r3d_name = factory->create_component_by_name("r3d_al", "test.r3d", "r3d_c1", this);
    uvm_component* d_r3d_type = factory->create_component_by_type(drv_h_x::get_type(), "test.r3d", "r3d_c2", this);
    check(comp_tag(d_r3d_name) == "drv_b", "R3-D: by_name preserves earliest seq drv_b");
    check(comp_tag(d_r3d_type) == "drv_b", "R3-D: by_type preserves earliest seq drv_b");

    factory->set_inst_override_by_type(drv_h_x::get_type(), drv_b::get_type(), "test.r3d_iso.*");
    factory->set_inst_alias("r3d_iso_al", drv_a::get_type(), "test.r3d_iso.a.*");
    factory->set_inst_override_by_name("r3d_iso_al", "drv_c_ovr", "test.r3d_iso.a.*");
    factory->set_inst_override_by_name("r3d_iso_al", "drv_b", "test.r3d_iso.*");
    factory->set_inst_alias("r3d_iso_al", drv_h_x::get_type(), "test.r3d_iso.*");
    uvm_component* d_r3d_iso_n = factory->create_component_by_name("r3d_iso_al", "test.r3d_iso.a", "r3d_u1", this);
    uvm_component* d_r3d_iso_t = factory->create_component_by_type(drv_a::get_type(), "test.r3d_iso.a", "r3d_u2", this);
    check(comp_tag(d_r3d_iso_n) == "drv_c_ovr", "R3-D: req_name isolation by_name -> drv_c_ovr");
    check(comp_tag(d_r3d_iso_t) == "drv_c_ovr", "R3-D: req_name isolation by_type -> drv_c_ovr");

    // R3-E: multi-hop inst_alias (immediate + deferred) and wildcard registration-order symmetry
    factory->set_inst_alias("r3e_1", "r3e_2", "test.r3e.*");
    factory->set_inst_alias("r3e_2", drv_h_x::get_type(), "test.r3e.ag1.*");
    factory->set_inst_override_by_name("r3e_1", "drv_b", "test.r3e.*");
    uvm_component* d_r3e_n = factory->create_component_by_name("r3e_1", "test.r3e.ag1", "r3e_d1", this);
    uvm_component* d_r3e_t = factory->create_component_by_type(drv_h_x::get_type(), "test.r3e.ag1", "r3e_d2", this);
    uvm_component* d_r3e_m = factory->create_component_by_type(drv_h_x::get_type(), "test.r3e.ag2", "r3e_d3", this);
    check(comp_tag(d_r3e_n) == "drv_b", "R3-E: 2-hop inst_alias by_name -> drv_b");
    check(comp_tag(d_r3e_t) == "drv_b", "R3-E: 2-hop inst_alias by_type -> drv_b");
    check(comp_tag(d_r3e_m) == "drv_h_x", "R3-E: 2-hop inst_alias outside ag1 stays drv_h_x");

    factory->set_inst_alias("r3e_def2", "r3e_def1", "test.r3e_def.*");
    factory->set_inst_alias("r3e_def1", "late_mhop_target", "test.r3e_def.ag1.*");
    factory->set_inst_override_by_name("r3e_def2", "drv_b", "test.r3e_def.*");
    late_mhop_wrapper* late_mhop_w = new late_mhop_wrapper();
    factory->do_register(late_mhop_w);
    uvm_component* d_r3ed_n = factory->create_component_by_name("r3e_def2", "test.r3e_def.ag1", "r3ed_d1", this);
    uvm_component* d_r3ed_t = factory->create_component_by_type(late_mhop_w, "test.r3e_def.ag1", "r3ed_d2", this);
    uvm_component* d_r3ed_m = factory->create_component_by_type(late_mhop_w, "test.r3e_def.ag2", "r3ed_d3", this);
    check(comp_tag(d_r3ed_n) == "drv_b", "R3-E: deferred 2-hop inst_alias by_name -> drv_b");
    check(comp_tag(d_r3ed_t) == "drv_b", "R3-E: deferred 2-hop inst_alias by_type -> drv_b");
    check(comp_tag(d_r3ed_m) == "drv_base", "R3-E: deferred 2-hop inst_alias outside ag1 stays late_mhop_target");

    factory->set_inst_override_by_name("r3e_leg*", "drv_b", "test.r3e_w1.*");
    factory->set_inst_alias("r3e_hop1", "r3e_leg_drv", "test.r3e_w1.*");
    factory->set_inst_alias("r3e_leg_drv", drv_h_x::get_type(), "test.r3e_w1.ag1.*");
    uvm_component* d_r3ew1_n = factory->create_component_by_name("r3e_hop1", "test.r3e_w1.ag1", "r3ew1_d1", this);
    uvm_component* d_r3ew1_t = factory->create_component_by_type(drv_h_x::get_type(), "test.r3e_w1.ag1", "r3ew1_d2", this);
    check(comp_tag(d_r3ew1_n) == "drv_b", "R3-E: wildcard before aliases by_name -> drv_b");
    check(comp_tag(d_r3ew1_t) == "drv_b", "R3-E: wildcard before aliases by_type -> drv_b");

    factory->set_inst_alias("r3e_hop2", "r3e_leg2_drv", "test.r3e_w2.*");
    factory->set_inst_alias("r3e_leg2_drv", drv_h_x::get_type(), "test.r3e_w2.ag1.*");
    factory->set_inst_override_by_name("r3e_leg2*", "drv_b", "test.r3e_w2.*");
    uvm_component* d_r3ew2_n = factory->create_component_by_name("r3e_hop2", "test.r3e_w2.ag1", "r3ew2_d1", this);
    uvm_component* d_r3ew2_t = factory->create_component_by_type(drv_h_x::get_type(), "test.r3e_w2.ag1", "r3ew2_d2", this);
    check(comp_tag(d_r3ew2_n) == "drv_b", "R3-E: aliases before wildcard by_name -> drv_b");
    check(comp_tag(d_r3ew2_t) == "drv_b", "R3-E: aliases before wildcard by_type -> drv_b");

    // R3-F: debug_create_by_name on wildcard-matched name agrees with create without warning
    factory->set_inst_override_by_name("r3f_*", "drv_b", "test.r3f.*");
    uvm_component* d_r3f = factory->create_component_by_name("r3f_unreg", "test.r3f", "r3f_d1", this);
    check(comp_tag(d_r3f) == "drv_b", "R3-F: wildcard create_component_by_name -> drv_b");
    factory->debug_create_by_name("r3f_unreg", "test.r3f", "r3f_dbg");

    // 11. debug_create_by_name & print
    factory->debug_create_by_name("alias_orig_a", "", "dbg1");
    factory->debug_create_by_name("pkt_alias", "test.special", "dbg2");
    factory->print(0);

    // Clean up heap-allocated objects
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_type));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_inst_wild));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_inst_q));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_chain));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_delayed));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_p8));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_anon));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_bidir1));
    pkt_orig_b::type_id::destroy(o_bidir2);
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_b_alias));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_b_orig));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_b_type));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_c_alias));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_c_type));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_c_repl));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_dual_norm));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_dual_inst));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_leg1));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_leg2));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_star));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_r2e));
    pkt_base::type_id::destroy(dynamic_cast<pkt_base*>(o_r2e_def));

    std::cout << "factory_alias_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    uvm_test::report_phase(phase);
    check(ag1 != nullptr && ag1->ok, "drv_inst_alias ag?.drv -> drv_a");
    check(ag2 != nullptr && ag2->ok, "inst override on original + inst alias -> drv_a_ovr");
    std::cout << "agent_alias_checks_ok="
              << ((ag1 != nullptr && ag1->ok && ag2 != nullptr && ag2->ok) ? 1 : 0)
              << std::endl;
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
