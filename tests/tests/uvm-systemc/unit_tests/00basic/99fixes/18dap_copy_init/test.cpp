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

// Regression test for DAP classes:
// - uvm_set_before_get_dap<T> instantiability (get/try_get non-const)
// - do_copy() non-const override dispatch through uvm_object::copy()
// - m_value() default initialization for std::string and user structs

#include <systemc>
#include <uvm>

#include <iostream>
#include <sstream>
#include <string>

using namespace uvm;

struct dap_cfg
{
  int a;
  std::string tag;

  dap_cfg() : a(42), tag("init") {}

  bool operator==( const dap_cfg& rhs ) const
  {
    return a == rhs.a && tag == rhs.tag;
  }
};

inline std::ostream& operator<<( std::ostream& os, const dap_cfg& c )
{
  return os << c.a << ":" << c.tag;
}

class cpy_catcher : public uvm_report_catcher
{
 public:
  int gtl_cpy;
  int sld_cpy;
  int sbg_cpy;
  int docpy;

  cpy_catcher() : gtl_cpy(0), sld_cpy(0), sbg_cpy(0), docpy(0) {}

  virtual action_e do_catch()
  {
    const std::string id = get_id();
    if (id == "UVM/GET_TO_LOCK_DAP/CPY")
    {
      gtl_cpy++;
      set_severity(UVM_INFO);
      return THROW;
    }
    if (id == "UVM/SIMPLE_LOCK_DAP/CPY")
    {
      sld_cpy++;
      set_severity(UVM_INFO);
      return THROW;
    }
    if (id == "UVM/SET_BEFORE_GET_DAP/CPY")
    {
      sbg_cpy++;
      set_severity(UVM_INFO);
      return THROW;
    }
    if (id == "DOCPY")
    {
      docpy++;
      return THROW;
    }
    return THROW;
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    uvm_coreservice_t* cs = uvm_coreservice_t::get();
    uvm_root* top = cs->get_root();
    top->set_report_verbosity_level_hier(UVM_HIGH);

    cpy_catcher ctchr;
    uvm_report_cb::add(nullptr, &ctchr);

    // 1. copy() -> do_copy() virtual dispatch
    uvm_get_to_lock_dap<int> gtl_a("gtl_a"), gtl_b("gtl_b");
    uvm_simple_lock_dap<int> sld_a("sld_a"), sld_b("sld_b");
    uvm_set_before_get_dap<int> sbg_a("sbg_a"), sbg_b("sbg_b");

    gtl_a.set(11);
    sld_a.set(22);
    sbg_a.set(33);

    uvm_object& gtl_ref = gtl_b;
    uvm_object& sld_ref = sld_b;
    uvm_object& sbg_ref = sbg_b;

    gtl_ref.copy(gtl_a);
    sld_ref.copy(sld_a);
    sbg_ref.copy(sbg_a);

    uvm_report_cb::do_delete(nullptr, &ctchr);

    // 2. std::string default initialization & DAP semantics
    uvm_get_to_lock_dap<std::string> gtl_str("gtl_str");
    bool str_ok = (gtl_str.get() == "") && (!gtl_str.try_set("late"));

    uvm_simple_lock_dap<std::string> sld_str("sld_str");
    str_ok = str_ok && (sld_str.get() == "");
    sld_str.set("abc");
    sld_str.lock();
    str_ok = str_ok && (!sld_str.try_set("xyz")) && (sld_str.get() == "abc");
    sld_str.unlock();
    str_ok = str_ok && sld_str.try_set("xyz") && (sld_str.get() == "xyz");

    uvm_set_before_get_dap<std::string> sbg_str("sbg_str");
    std::string out_str = "sentinel";
    str_ok = str_ok && (!sbg_str.try_get(out_str)) && (out_str == "sentinel");
    sbg_str.set("hello");
    str_ok = str_ok && sbg_str.try_get(out_str) && (out_str == "hello") && (sbg_str.get() == "hello");

    // 3. User struct default initialization & convert2string
    uvm_get_to_lock_dap<dap_cfg> gtl_cfg("gtl_cfg");
    uvm_simple_lock_dap<dap_cfg> sld_cfg("sld_cfg");
    uvm_set_before_get_dap<dap_cfg> sbg_cfg("sbg_cfg");

    dap_cfg tmp_cfg;
    bool cfg_ok = (gtl_cfg.get() == dap_cfg()) &&
                  (sld_cfg.get() == dap_cfg()) &&
                  (!sbg_cfg.try_get(tmp_cfg));
    dap_cfg updated;
    updated.a = 99;
    updated.tag = "set";
    sbg_cfg.set(updated);
    cfg_ok = cfg_ok && sbg_cfg.try_get(tmp_cfg) && (tmp_cfg == updated) &&
             (sbg_cfg.get() == updated);

    std::cout << "cpy_counts: gtl=" << ctchr.gtl_cpy
              << " sld=" << ctchr.sld_cpy
              << " sbg=" << ctchr.sbg_cpy
              << " docpy=" << ctchr.docpy << std::endl;
    std::cout << "str_ok=" << str_ok << " cfg_ok=" << cfg_ok << std::endl;
    std::cout << "gtl_cfg=" << gtl_cfg.convert2string() << std::endl;
    std::cout << "sld_cfg=" << sld_cfg.convert2string() << std::endl;
    std::cout << "sbg_cfg=" << sbg_cfg.convert2string() << std::endl;

    if (ctchr.gtl_cpy == 1 && ctchr.sld_cpy == 1 && ctchr.sbg_cpy == 1 &&
        ctchr.docpy == 0 && str_ok && cfg_ok)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "!! UVM TEST FAILED !!" << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
