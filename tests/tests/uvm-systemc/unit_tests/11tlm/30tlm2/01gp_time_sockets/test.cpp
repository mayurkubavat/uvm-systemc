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

// Unit test for src/uvmsc/tlm2/:
// - uvm_tlm_time (resolution, incr/decr, realtime/abstime, sc_time interop, error guards)
// - uvm_tlm_gp / uvm_tlm_generic_payload (factory, copy/clone, compare, pack/unpack,
//   print/sprint table+JSON+XML, record, convert2string, uvm_tlm_extension<T>,
//   tlm::tlm_generic_payload interop)
// - Blocking UVM TLM-2 sockets & passthrough hierarchy + unbound fatal check
// - Non-blocking UVM TLM-2 sockets & passthrough hierarchy (4-phase + early completed)
// - SystemC 3.0.3 TLM-2 socket wrappers & UVM<->SystemC bridges

#include <systemc>
#include <tlm>
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include <uvm>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace uvm;

class tlm2_msg_catcher : public uvm_report_catcher
{
 public:
  int error_count = 0;
  int fatal_count = 0;

  virtual action_e do_catch()
  {
    const std::string id = get_id();
    if (get_severity() == UVM_ERROR &&
        (id == "UVM/TLM/BADRES" || id == "UVM/TLM/NEGTIME" || id == "UVM/TLM/BADSCALE"))
    {
      ++error_count;
      return CAUGHT;
    }
    if (get_severity() == UVM_FATAL && id == "UVM/TLM2/UNBOUND")
    {
      ++fatal_count;
      set_severity(UVM_INFO);
      return CAUGHT;
    }
    return THROW;
  }
};

class my_bus_ext : public uvm_tlm_extension<my_bus_ext>
{
 public:
  UVM_OBJECT_UTILS(my_bus_ext);

  unsigned int domain_id;
  bool secure;

  explicit my_bus_ext(const std::string& name = "my_bus_ext")
    : uvm_tlm_extension<my_bus_ext>(name), domain_id(0), secure(false) {}

  my_bus_ext(const my_bus_ext& rhs)
    : uvm_tlm_extension<my_bus_ext>(rhs),
      domain_id(rhs.domain_id),
      secure(rhs.secure) {}

  virtual uvm_object* create(const std::string& name = "")
  {
    return new my_bus_ext(name);
  }

  virtual void do_copy(const uvm_object& rhs)
  {
    const my_bus_ext* r = dynamic_cast<const my_bus_ext*>(&rhs);
    if (r != nullptr)
    {
      domain_id = r->domain_id;
      secure = r->secure;
    }
  }

  virtual bool do_compare(const uvm_object& rhs, const uvm_comparer* comparer) const
  {
    const uvm_comparer* eff = (comparer != nullptr) ? comparer : uvm_comparer::get_default();
    const my_bus_ext* r = dynamic_cast<const my_bus_ext*>(&rhs);
    if (r == nullptr)
      return false;
    bool ok = true;
    ok &= eff->compare_field_int("domain_id", domain_id, r->domain_id, 32, UVM_DEC);
    ok &= eff->compare_field_int("secure", secure ? 1 : 0, r->secure ? 1 : 0, 1, UVM_BIN);
    return ok;
  }

  virtual void do_print(const uvm_printer& printer) const
  {
    printer.print_field_int("domain_id", domain_id, 32, UVM_DEC);
    printer.print_field_int("secure", secure ? 1 : 0, 1, UVM_BIN);
  }
};

// Blocking UVM TLM-2 target & passthrough hierarchy
class b_target_comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(b_target_comp);

  uvm_tlm_b_target_socket<uvm_tlm_gp, b_target_comp> target_skt;
  std::vector<unsigned char> mem;

  explicit b_target_comp(uvm_component_name name)
    : uvm_component(name),
      target_skt("target_skt", this),
      mem(256, 0) {}

  void b_transport(uvm_tlm_gp& gp, uvm_tlm_time& delay)
  {
    sc_dt::uint64 addr = gp.get_address();
    unsigned int len = gp.get_data_length();
    if (addr + len > mem.size())
    {
      gp.set_response_status(UVM_TLM_ADDRESS_ERROR_RESPONSE);
      return;
    }
    std::vector<unsigned char> data;
    gp.get_data(data);
    std::vector<unsigned char> be;
    gp.get_byte_enable(be);
    unsigned int be_len = gp.get_byte_enable_length();

    if (gp.is_write())
    {
      for (unsigned int i = 0; i < len && i < data.size(); ++i)
      {
        bool en = (be_len == 0 || be.empty()) ||
                  (be[i % be_len] != UVM_TLM_BYTE_DISABLED);
        if (en)
          mem[addr + i] = data[i];
      }
      gp.set_response_status(UVM_TLM_OK_RESPONSE);
    }
    else if (gp.is_read())
    {
      data.resize(len);
      for (unsigned int i = 0; i < len; ++i)
        data[i] = mem[addr + i];
      gp.set_data(data);
      gp.set_response_status(UVM_TLM_OK_RESPONSE);
    }
    delay.incr(5.0, 1.0e-9);
  }
};

class b_initiator_comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(b_initiator_comp);

  uvm_tlm_b_initiator_socket<uvm_tlm_gp> init_skt;

  explicit b_initiator_comp(uvm_component_name name)
    : uvm_component(name),
      init_skt("init_skt") {}
};

class b_init_wrapper : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(b_init_wrapper);

  b_initiator_comp* child;
  uvm_tlm_b_passthrough_initiator_socket<uvm_tlm_gp> pt_init;

  explicit b_init_wrapper(uvm_component_name name)
    : uvm_component(name),
      child(nullptr),
      pt_init("pt_init") {}

  virtual void build_phase(uvm_phase& phase)
  {
    uvm_component::build_phase(phase);
    child = b_initiator_comp::type_id::create("child", this);
  }

  virtual void connect_phase(uvm_phase& phase)
  {
    uvm_component::connect_phase(phase);
    child->init_skt.connect(pt_init);
  }
};

class b_tgt_wrapper : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(b_tgt_wrapper);

  b_target_comp* child;
  uvm_tlm_b_passthrough_target_socket<uvm_tlm_gp> pt_tgt;

  explicit b_tgt_wrapper(uvm_component_name name)
    : uvm_component(name),
      child(nullptr),
      pt_tgt("pt_tgt") {}

  virtual void build_phase(uvm_phase& phase)
  {
    uvm_component::build_phase(phase);
    child = b_target_comp::type_id::create("child", this);
  }

  virtual void connect_phase(uvm_phase& phase)
  {
    uvm_component::connect_phase(phase);
    pt_tgt.connect(child->target_skt);
  }
};

// Non-blocking UVM TLM-2 initiator & target & passthrough hierarchy
class nb_initiator_comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(nb_initiator_comp);

  uvm_tlm_nb_initiator_socket<uvm_tlm_gp, uvm_tlm_phase_e, nb_initiator_comp> init_skt;
  sc_core::sc_event resp_ev;
  int bw_calls = 0;
  bool got_end_req = false;
  bool got_begin_resp = false;

  explicit nb_initiator_comp(uvm_component_name name)
    : uvm_component(name),
      init_skt("init_skt", this) {}

  uvm_tlm_sync_e nb_transport_bw(uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay)
  {
    ++bw_calls;
    delay.incr(1.0, 1.0e-9);
    if (phase == END_REQ)
    {
      got_end_req = true;
      return UVM_TLM_ACCEPTED;
    }
    if (phase == BEGIN_RESP)
    {
      got_begin_resp = true;
      gp.set_response_status(UVM_TLM_OK_RESPONSE);
      phase = END_RESP;
      resp_ev.notify(sc_core::SC_ZERO_TIME);
      return UVM_TLM_COMPLETED;
    }
    return UVM_TLM_ACCEPTED;
  }
};

class nb_target_comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(nb_target_comp);

  uvm_tlm_nb_target_socket<uvm_tlm_gp, uvm_tlm_phase_e, nb_target_comp> target_skt;
  bool early_complete = false;
  uvm_tlm_gp* pending_gp = nullptr;
  sc_core::sc_event req_ev;

  explicit nb_target_comp(uvm_component_name name)
    : uvm_component(name),
      target_skt("target_skt", this) {}

  uvm_tlm_sync_e nb_transport_fw(uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay)
  {
    delay.incr(2.0, 1.0e-9);
    if (phase == BEGIN_REQ)
    {
      if (early_complete)
      {
        gp.set_response_status(UVM_TLM_OK_RESPONSE);
        phase = END_RESP;
        return UVM_TLM_COMPLETED;
      }
      pending_gp = &gp;
      req_ev.notify(sc_core::SC_ZERO_TIME);
      return UVM_TLM_ACCEPTED;
    }
    return UVM_TLM_ACCEPTED;
  }

  virtual void run_phase(uvm_phase&)
  {
    while (true)
    {
      sc_core::wait(req_ev);
      if (pending_gp == nullptr)
        continue;
      uvm_tlm_gp* gp = pending_gp;
      pending_gp = nullptr;

      uvm_tlm_time d("d");
      uvm_tlm_phase_e ph = END_REQ;
      target_skt.nb_transport_bw(*gp, ph, d);

      sc_core::wait(2, sc_core::SC_NS);
      ph = BEGIN_RESP;
      gp->set_response_status(UVM_TLM_OK_RESPONSE);
      target_skt.nb_transport_bw(*gp, ph, d);
    }
  }
};

class nb_init_wrapper : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(nb_init_wrapper);

  nb_initiator_comp* child;
  uvm_tlm_nb_passthrough_initiator_socket<uvm_tlm_gp, uvm_tlm_phase_e> pt_init;

  explicit nb_init_wrapper(uvm_component_name name)
    : uvm_component(name),
      child(nullptr),
      pt_init("pt_init") {}

  virtual void build_phase(uvm_phase& phase)
  {
    uvm_component::build_phase(phase);
    child = nb_initiator_comp::type_id::create("child", this);
  }

  virtual void connect_phase(uvm_phase& phase)
  {
    uvm_component::connect_phase(phase);
    child->init_skt.connect(pt_init);
  }
};

class nb_tgt_wrapper : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(nb_tgt_wrapper);

  nb_target_comp* child;
  uvm_tlm_nb_passthrough_target_socket<uvm_tlm_gp, uvm_tlm_phase_e> pt_tgt;

  explicit nb_tgt_wrapper(uvm_component_name name)
    : uvm_component(name),
      child(nullptr),
      pt_tgt("pt_tgt") {}

  virtual void build_phase(uvm_phase& phase)
  {
    uvm_component::build_phase(phase);
    child = nb_target_comp::type_id::create("child", this);
  }

  virtual void connect_phase(uvm_phase& phase)
  {
    uvm_component::connect_phase(phase);
    pt_tgt.connect(child->target_skt);
  }
};

// Native SystemC TLM-2 target & initiator for interop bridge tests
class sysc_target_mod : public sc_core::sc_module
{
 public:
  tlm_utils::simple_target_socket<sysc_target_mod> skt;
  std::vector<unsigned char> mem;

  explicit sysc_target_mod(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      skt("skt"),
      mem(64, 0)
  {
    skt.register_b_transport(this, &sysc_target_mod::b_transport);
  }

  void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
  {
    sc_dt::uint64 addr = trans.get_address();
    unsigned int len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();
    unsigned char* be_ptr = trans.get_byte_enable_ptr();
    unsigned int be_len = trans.get_byte_enable_length();
    if (addr + len > mem.size() || ptr == nullptr)
    {
      trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
      return;
    }
    if (trans.is_write())
    {
      for (unsigned int i = 0; i < len; ++i)
      {
        bool en = (be_ptr == nullptr || be_len == 0) ||
                  (be_ptr[i % be_len] != TLM_BYTE_DISABLED);
        if (en)
          mem[addr + i] = ptr[i];
      }
    }
    else if (trans.is_read())
    {
      for (unsigned int i = 0; i < len; ++i)
      {
        bool en = (be_ptr == nullptr || be_len == 0) ||
                  (be_ptr[i % be_len] != TLM_BYTE_DISABLED);
        if (en)
          ptr[i] = mem[addr + i];
      }
    }
    delay += sc_core::sc_time(3, sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
  }
};

class sysc_init_mod : public sc_core::sc_module
{
 public:
  tlm_utils::simple_initiator_socket<sysc_init_mod> skt;

  explicit sysc_init_mod(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      skt("skt") {}
};

class dummy_sc_bw_if : public virtual tlm::tlm_bw_transport_if<>
{
 public:
  virtual tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload&,
                                              tlm::tlm_phase&,
                                              sc_core::sc_time&)
  {
    return tlm::TLM_COMPLETED;
  }
  virtual void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) {}
};

// Direct imp helper classes (not inheriting uvm_tlm_if_base)
class b_imp_target_obj
{
 public:
  int calls = 0;
  sc_dt::uint64 last_addr = 0;

  void b_transport(uvm_tlm_gp& gp, uvm_tlm_time& delay)
  {
    ++calls;
    last_addr = gp.get_address();
    if (gp.is_read())
    {
      gp.set_data({0xA1, 0xB2, 0xC3, 0xD4});
    }
    gp.set_response_status(UVM_TLM_OK_RESPONSE);
    delay.incr(1.0, 1.0e-9);
  }
};

class nb_imp_peer_obj
{
 public:
  int fw_calls = 0;
  int bw_calls = 0;

  uvm_tlm_sync_e nb_transport_fw(uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay)
  {
    ++fw_calls;
    delay.incr(1.0, 1.0e-9);
    if (phase == BEGIN_REQ)
    {
      gp.set_response_status(UVM_TLM_OK_RESPONSE);
      phase = END_RESP;
      return UVM_TLM_COMPLETED;
    }
    return UVM_TLM_ACCEPTED;
  }

  uvm_tlm_sync_e nb_transport_bw(uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay)
  {
    (void)gp;
    (void)phase;
    ++bw_calls;
    delay.incr(1.0, 1.0e-9);
    return UVM_TLM_COMPLETED;
  }
};

// UVM NB initiator driver for bridge NB test (4-phase + phase clearing data)
class bridge_nb_uvm_init : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(bridge_nb_uvm_init);

  uvm_tlm_nb_initiator_socket<uvm_tlm_gp, uvm_tlm_phase_e, bridge_nb_uvm_init> init_skt;
  sc_core::sc_event done_ev;
  bool got_end_req = false;
  bool got_begin_resp = false;

  explicit bridge_nb_uvm_init(uvm_component_name name)
    : uvm_component(name),
      init_skt("init_skt", this) {}

  uvm_tlm_sync_e nb_transport_bw(uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay)
  {
    delay.incr(1.0, 1.0e-9);
    if (phase == END_REQ)
    {
      got_end_req = true;
      // Transition gp.m_data from non-empty to empty on intermediate bw phase
      // when testing data->empty transition (address == 0x34)
      if (gp.get_address() == 0x34)
      {
        gp.m_data.clear();
        gp.set_data_length(0);
      }
      if (gp.get_address() == 0x28)
      {
        gp.set_byte_enable_length(4);
        gp.set_byte_enable({UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED});
      }
      return UVM_TLM_ACCEPTED;
    }
    if (phase == BEGIN_RESP)
    {
      got_begin_resp = true;
      phase = END_RESP;
      done_ev.notify(sc_core::SC_ZERO_TIME);
      return UVM_TLM_COMPLETED;
    }
    return UVM_TLM_ACCEPTED;
  }
};

// SystemC NB target module (4-phase BEGIN_REQ -> END_REQ -> BEGIN_RESP -> END_RESP)
class sysc_nb_target_mod : public sc_core::sc_module,
                           public virtual tlm::tlm_fw_transport_if<>
{
 public:
  tlm::tlm_target_socket<32> skt;
  std::vector<unsigned char> mem;
  tlm::tlm_generic_payload* pending_gp = nullptr;
  sc_core::sc_event req_ev;
  bool saw_null_ptr = false;

  explicit sysc_nb_target_mod(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      skt("skt"),
      mem(64, 0)
  {
    skt.bind(*this);
    SC_THREAD(process_thread);
  }

  virtual void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
  {
    (void)delay;
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  virtual tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans,
                                              tlm::tlm_phase& phase,
                                              sc_core::sc_time& delay)
  {
    if (phase == tlm::BEGIN_REQ)
    {
      if (trans.get_data_length() > 0 && trans.get_data_ptr() == nullptr)
      {
        saw_null_ptr = true;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return tlm::TLM_COMPLETED;
      }
      if (trans.is_write() && trans.get_data_ptr() != nullptr)
      {
        sc_dt::uint64 addr = trans.get_address();
        unsigned int len = trans.get_data_length();
        unsigned char* be_ptr = trans.get_byte_enable_ptr();
        unsigned int be_len = trans.get_byte_enable_length();
        for (unsigned int i = 0; i < len && (addr + i) < mem.size(); ++i)
        {
          bool en = (be_ptr == nullptr || be_len == 0) ||
                    (be_ptr[i % be_len] != TLM_BYTE_DISABLED);
          if (en)
            mem[addr + i] = trans.get_data_ptr()[i];
        }
      }
      pending_gp = &trans;
      req_ev.notify(delay);
      return tlm::TLM_ACCEPTED;
    }
    return tlm::TLM_ACCEPTED;
  }

  void process_thread()
  {
    while (true)
    {
      sc_core::wait(req_ev);
      if (pending_gp == nullptr)
        continue;
      tlm::tlm_generic_payload* gp = pending_gp;
      pending_gp = nullptr;

      tlm::tlm_phase ph = tlm::END_REQ;
      sc_core::sc_time d = sc_core::SC_ZERO_TIME;
      skt->nb_transport_bw(*gp, ph, d);

      sc_core::wait(1, sc_core::SC_NS);
      if (gp->is_read() && gp->get_data_ptr() != nullptr)
      {
        sc_dt::uint64 addr = gp->get_address();
        unsigned int len = gp->get_data_length();
        for (unsigned int i = 0; i < len && (addr + i) < mem.size(); ++i)
          gp->get_data_ptr()[i] = mem[addr + i];
      }
      gp->set_response_status(tlm::TLM_OK_RESPONSE);
      ph = tlm::BEGIN_RESP;
      skt->nb_transport_bw(*gp, ph, d);
    }
  }

  virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload&, tlm::tlm_dmi&) { return false; }
  virtual unsigned int transport_dbg(tlm::tlm_generic_payload&) { return 0; }
};

// SystemC NB initiator module for testing uvm_tlm_sc_target_bridge NB path
class sysc_nb_init_mod : public sc_core::sc_module,
                         public virtual tlm::tlm_bw_transport_if<>
{
 public:
  tlm::tlm_initiator_socket<32> skt;
  sc_core::sc_event done_ev;
  bool got_end_req = false;
  bool got_begin_resp = false;

  explicit sysc_nb_init_mod(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      skt("skt")
  {
    skt.bind(*this);
  }

  virtual tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                              tlm::tlm_phase& phase,
                                              sc_core::sc_time& delay)
  {
    (void)trans;
    (void)delay;
    if (phase == tlm::END_REQ)
    {
      got_end_req = true;
      return tlm::TLM_ACCEPTED;
    }
    if (phase == tlm::BEGIN_RESP)
    {
      got_begin_resp = true;
      phase = tlm::END_RESP;
      done_ev.notify(sc_core::SC_ZERO_TIME);
      return tlm::TLM_COMPLETED;
    }
    return tlm::TLM_ACCEPTED;
  }

  virtual void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) {}
};

// UVM target that populates m_data even when incoming m_data was empty
class uvm_null_ptr_target_comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(uvm_null_ptr_target_comp);

  uvm_tlm_b_target_socket<uvm_tlm_gp, uvm_null_ptr_target_comp> b_skt;
  uvm_tlm_nb_target_socket<uvm_tlm_gp, uvm_tlm_phase_e, uvm_null_ptr_target_comp> nb_skt;

  explicit uvm_null_ptr_target_comp(uvm_component_name name)
    : uvm_component(name),
      b_skt("b_skt", this),
      nb_skt("nb_skt", this) {}

  void b_transport(uvm_tlm_gp& gp, uvm_tlm_time& delay)
  {
    (void)delay;
    gp.set_data({0xDE, 0xAD, 0xBE, 0xEF});
    gp.set_byte_enable({UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED});
    gp.set_response_status(UVM_TLM_OK_RESPONSE);
  }

  uvm_tlm_sync_e nb_transport_fw(uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay)
  {
    (void)delay;
    if (phase == BEGIN_REQ)
    {
      gp.set_data({0xDE, 0xAD, 0xBE, 0xEF});
      gp.set_byte_enable({UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED});
      gp.set_response_status(UVM_TLM_OK_RESPONSE);
      phase = END_RESP;
      return UVM_TLM_COMPLETED;
    }
    return UVM_TLM_ACCEPTED;
  }
};

// UVM target that returns oversized m_data (8 bytes) & m_byte_enable (4 bytes)
class uvm_oversized_resp_target_comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(uvm_oversized_resp_target_comp);

  uvm_tlm_b_target_socket<uvm_tlm_gp, uvm_oversized_resp_target_comp> b_skt;
  uvm_tlm_nb_target_socket<uvm_tlm_gp, uvm_tlm_phase_e, uvm_oversized_resp_target_comp> nb_skt;
  uvm_tlm_gp* pending_gp = nullptr;
  sc_core::sc_event req_ev;

  explicit uvm_oversized_resp_target_comp(uvm_component_name name)
    : uvm_component(name),
      b_skt("b_skt", this),
      nb_skt("nb_skt", this) {}

  void b_transport(uvm_tlm_gp& gp, uvm_tlm_time& delay)
  {
    (void)delay;
    gp.set_data_length(8);
    gp.set_data({0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80});
    gp.set_byte_enable_length(4);
    gp.set_byte_enable({UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED,
                        UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED});
    gp.set_response_status(UVM_TLM_OK_RESPONSE);
  }

  uvm_tlm_sync_e nb_transport_fw(uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay)
  {
    (void)delay;
    if (phase == BEGIN_REQ)
    {
      pending_gp = &gp;
      req_ev.notify(sc_core::SC_ZERO_TIME);
      return UVM_TLM_ACCEPTED;
    }
    return UVM_TLM_ACCEPTED;
  }

  virtual void run_phase(uvm_phase&)
  {
    while (true)
    {
      sc_core::wait(req_ev);
      if (pending_gp == nullptr)
        continue;
      uvm_tlm_gp* gp = pending_gp;
      pending_gp = nullptr;

      uvm_tlm_time d("d");
      uvm_tlm_phase_e ph = END_REQ;
      nb_skt.nb_transport_bw(*gp, ph, d);

      sc_core::wait(sc_core::SC_ZERO_TIME);
      gp->set_data_length(8);
      gp->set_data({0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x01, 0x02});
      gp->set_byte_enable_length(4);
      gp->set_byte_enable({UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED,
                           UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED});
      gp->set_response_status(UVM_TLM_OK_RESPONSE);
      ph = BEGIN_RESP;
      nb_skt.nb_transport_bw(*gp, ph, d);
    }
  }
};

// Re-entrant NB test helpers (Section 7f)
class reentrant_sc_init_mod : public sc_core::sc_module,
                              public virtual tlm::tlm_bw_transport_if<>
{
 public:
  tlm::tlm_initiator_socket<32> skt;
  sc_core::sc_event done_ev;
  bool call_end_resp_in_bw = false;
  int bw_calls = 0;
  bool saw_ok = false;
  std::vector<unsigned char> rx_data;
  tlm::tlm_sync_enum end_resp_fw_sync = tlm::TLM_ACCEPTED;

  explicit reentrant_sc_init_mod(sc_core::sc_module_name name)
    : sc_core::sc_module(name), skt("skt")
  {
    skt.bind(*this);
  }

  virtual tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                              tlm::tlm_phase& phase,
                                              sc_core::sc_time& delay)
  {
    ++bw_calls;
    if (phase == tlm::BEGIN_RESP)
    {
      saw_ok = trans.is_response_ok();
      rx_data.clear();
      if (trans.get_data_ptr() != nullptr)
        rx_data.assign(trans.get_data_ptr(), trans.get_data_ptr() + trans.get_data_length());
      if (call_end_resp_in_bw)
      {
        tlm::tlm_phase end_ph = tlm::END_RESP;
        end_resp_fw_sync = skt->nb_transport_fw(trans, end_ph, delay);
        done_ev.notify(sc_core::SC_ZERO_TIME);
        return tlm::TLM_ACCEPTED;
      }
      phase = tlm::END_RESP;
      return tlm::TLM_COMPLETED;
    }
    return tlm::TLM_ACCEPTED;
  }

  virtual void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) {}
};

class reentrant_uvm_tgt_comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(reentrant_uvm_tgt_comp);

  uvm_tlm_nb_target_socket<uvm_tlm_gp, uvm_tlm_phase_e, reentrant_uvm_tgt_comp> nb_skt;
  sc_core::sc_event req_ev;
  uvm_tlm_gp* pending_gp = nullptr;
  bool sync_bw_in_fw = true;
  bool end_resp_seen = false;
  uvm_tlm_sync_e bw_ret_sync = UVM_TLM_ACCEPTED;

  explicit reentrant_uvm_tgt_comp(uvm_component_name name)
    : uvm_component(name), nb_skt("nb_skt", this) {}

  uvm_tlm_sync_e nb_transport_fw(uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay)
  {
    if (phase == BEGIN_REQ)
    {
      if (sync_bw_in_fw)
      {
        gp.set_data({0xC1, 0xC2, 0xC3, 0xC4});
        gp.set_response_status(UVM_TLM_OK_RESPONSE);
        uvm_tlm_phase_e bw_ph = BEGIN_RESP;
        bw_ret_sync = nb_skt.nb_transport_bw(gp, bw_ph, delay);
        return UVM_TLM_ACCEPTED;
      }
      pending_gp = &gp;
      req_ev.notify(sc_core::SC_ZERO_TIME);
      return UVM_TLM_ACCEPTED;
    }
    if (phase == END_RESP)
    {
      end_resp_seen = true;
      return UVM_TLM_COMPLETED;
    }
    return UVM_TLM_ACCEPTED;
  }

  virtual void run_phase(uvm_phase&)
  {
    while (true)
    {
      sc_core::wait(req_ev);
      if (pending_gp == nullptr)
        continue;
      uvm_tlm_gp* gp = pending_gp;
      pending_gp = nullptr;
      gp->set_data({0xD1, 0xD2, 0xD3, 0xD4});
      gp->set_response_status(UVM_TLM_OK_RESPONSE);
      uvm_tlm_phase_e ph = BEGIN_RESP;
      uvm_tlm_time d("d");
      bw_ret_sync = nb_skt.nb_transport_bw(*gp, ph, d);
    }
  }
};

class reentrant_uvm_init_comp : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(reentrant_uvm_init_comp);

  uvm_tlm_nb_initiator_socket<uvm_tlm_gp, uvm_tlm_phase_e, reentrant_uvm_init_comp> nb_skt;
  sc_core::sc_event done_ev;
  bool call_end_resp_in_bw = false;
  int bw_calls = 0;
  bool saw_ok = false;
  std::vector<unsigned char> rx_data;
  uvm_tlm_sync_e end_resp_fw_sync = UVM_TLM_ACCEPTED;

  explicit reentrant_uvm_init_comp(uvm_component_name name)
    : uvm_component(name), nb_skt("nb_skt", this) {}

  uvm_tlm_sync_e nb_transport_bw(uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay)
  {
    ++bw_calls;
    if (phase == BEGIN_RESP)
    {
      saw_ok = gp.is_response_ok();
      rx_data = gp.m_data;
      if (call_end_resp_in_bw)
      {
        uvm_tlm_phase_e end_ph = END_RESP;
        end_resp_fw_sync = nb_skt.nb_transport_fw(gp, end_ph, delay);
        done_ev.notify(sc_core::SC_ZERO_TIME);
        return UVM_TLM_ACCEPTED;
      }
      phase = END_RESP;
      return UVM_TLM_COMPLETED;
    }
    return UVM_TLM_ACCEPTED;
  }
};

class reentrant_sc_tgt_mod : public sc_core::sc_module,
                             public virtual tlm::tlm_fw_transport_if<>
{
 public:
  tlm::tlm_target_socket<32> skt;
  sc_core::sc_event req_ev;
  tlm::tlm_generic_payload* pending_gp = nullptr;
  bool sync_bw_in_fw = true;
  bool end_resp_seen = false;
  tlm::tlm_sync_enum bw_ret_sync = tlm::TLM_ACCEPTED;

  explicit reentrant_sc_tgt_mod(sc_core::sc_module_name name)
    : sc_core::sc_module(name), skt("skt")
  {
    skt.bind(*this);
    SC_THREAD(process_thread);
  }

  virtual void b_transport(tlm::tlm_generic_payload&, sc_core::sc_time&) {}

  virtual tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans,
                                              tlm::tlm_phase& phase,
                                              sc_core::sc_time& delay)
  {
    if (phase == tlm::BEGIN_REQ)
    {
      if (sync_bw_in_fw)
      {
        if (trans.is_read() && trans.get_data_ptr() != nullptr)
        {
          const unsigned char resp[4] = {0xE1, 0xE2, 0xE3, 0xE4};
          for (unsigned int i = 0; i < trans.get_data_length() && i < 4; ++i)
            trans.get_data_ptr()[i] = resp[i];
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        tlm::tlm_phase bw_ph = tlm::BEGIN_RESP;
        bw_ret_sync = skt->nb_transport_bw(trans, bw_ph, delay);
        return tlm::TLM_ACCEPTED;
      }
      pending_gp = &trans;
      req_ev.notify(sc_core::SC_ZERO_TIME);
      return tlm::TLM_ACCEPTED;
    }
    if (phase == tlm::END_RESP)
    {
      end_resp_seen = true;
      return tlm::TLM_COMPLETED;
    }
    return tlm::TLM_ACCEPTED;
  }

  void process_thread()
  {
    while (true)
    {
      sc_core::wait(req_ev);
      if (pending_gp == nullptr)
        continue;
      tlm::tlm_generic_payload* gp = pending_gp;
      pending_gp = nullptr;
      if (gp->is_read() && gp->get_data_ptr() != nullptr)
      {
        const unsigned char resp[4] = {0xF1, 0xF2, 0xF3, 0xF4};
        for (unsigned int i = 0; i < gp->get_data_length() && i < 4; ++i)
          gp->get_data_ptr()[i] = resp[i];
      }
      gp->set_response_status(tlm::TLM_OK_RESPONSE);
      tlm::tlm_phase ph = tlm::BEGIN_RESP;
      sc_core::sc_time d = sc_core::SC_ZERO_TIME;
      bw_ret_sync = skt->nb_transport_bw(*gp, ph, d);
    }
  }

  virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload&, tlm::tlm_dmi&) { return false; }
  virtual unsigned int transport_dbg(tlm::tlm_generic_payload&) { return 0; }
};

class tlm2_test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(tlm2_test);

  b_init_wrapper* b_init_w = nullptr;
  b_tgt_wrapper* b_tgt_w = nullptr;
  uvm_tlm_b_initiator_socket<uvm_tlm_gp> unbound_b_init;

  nb_init_wrapper* nb_init_w = nullptr;
  nb_tgt_wrapper* nb_tgt_w = nullptr;

  // SystemC TLM-2 interop
  uvm_tlm_b_initiator_socket<uvm_tlm_gp> uvm_to_sc_init;
  uvm_tlm_sc_initiator_bridge<32> sc_init_bridge;
  sysc_target_mod sc_tgt;

  sysc_init_mod sc_init;
  uvm_tlm_sc_target_bridge<32> sc_tgt_bridge;
  b_target_comp* uvm_tgt_from_sc = nullptr;

  uvm_tlm_sc_initiator_socket<32> sc_wrap_init;
  uvm_tlm_sc_target_socket<32> sc_wrap_tgt;
  dummy_sc_bw_if sc_wrap_bw;
  sysc_target_mod sc_wrap_target_mod;

  // Direct imp & socket connection tests
  b_imp_target_obj b_imp_obj;
  uvm_tlm_b_transport_imp<uvm_tlm_gp, b_imp_target_obj> b_imp;
  uvm_tlm_b_initiator_socket<uvm_tlm_gp> imp_b_init_skt;
  uvm_tlm_b_target_socket<uvm_tlm_gp> imp_b_tgt_skt;

  nb_imp_peer_obj nb_imp_obj;
  uvm_tlm_nb_transport_fw_imp<uvm_tlm_gp, uvm_tlm_phase_e, nb_imp_peer_obj> nb_fw_imp;
  uvm_tlm_nb_transport_bw_imp<uvm_tlm_gp, uvm_tlm_phase_e, nb_imp_peer_obj> nb_bw_imp;
  uvm_tlm_nb_transport_imp<uvm_tlm_gp, uvm_tlm_phase_e, nb_imp_peer_obj> nb_both_imp;
  uvm_tlm_nb_initiator_socket<uvm_tlm_gp, uvm_tlm_phase_e> imp_nb_init_skt;
  uvm_tlm_nb_target_socket<uvm_tlm_gp, uvm_tlm_phase_e> imp_nb_tgt_skt;
  uvm_tlm_nb_initiator_socket<uvm_tlm_gp, uvm_tlm_phase_e> imp_nb_init_both_skt;
  uvm_tlm_nb_target_socket<uvm_tlm_gp, uvm_tlm_phase_e> imp_nb_tgt_both_skt;

  // NB SystemC bridges & null-ptr UAF regression tests
  bridge_nb_uvm_init* bridge_nb_init = nullptr;
  uvm_tlm_sc_initiator_bridge<32> sc_nb_init_bridge;
  sysc_nb_target_mod sc_nb_tgt;

  sysc_nb_init_mod sc_nb_init;
  uvm_tlm_sc_target_bridge<32> sc_nb_tgt_bridge;
  nb_target_comp* uvm_nb_tgt_from_sc = nullptr;

  sysc_nb_init_mod sc_null_init;
  uvm_tlm_sc_target_bridge<32> sc_null_tgt_bridge;
  uvm_null_ptr_target_comp* uvm_null_tgt = nullptr;

  sysc_nb_init_mod sc_undersized_init;
  uvm_tlm_sc_target_bridge<32> sc_undersized_tgt_bridge;
  uvm_oversized_resp_target_comp* uvm_oversized_tgt = nullptr;

  reentrant_sc_init_mod sc_reent_init;
  uvm_tlm_sc_target_bridge<32> sc_reent_tgt_bridge;
  reentrant_uvm_tgt_comp* uvm_reent_tgt = nullptr;

  reentrant_uvm_init_comp* uvm_reent_init = nullptr;
  uvm_tlm_sc_initiator_bridge<32> sc_reent_init_bridge;
  reentrant_sc_tgt_mod sc_reent_tgt;

  bool all_ok = true;

  explicit tlm2_test(uvm_component_name name)
    : uvm_test(name),
      unbound_b_init("unbound_b_init"),
      uvm_to_sc_init("uvm_to_sc_init"),
      sc_init_bridge("sc_init_bridge"),
      sc_tgt("sc_tgt"),
      sc_init("sc_init"),
      sc_tgt_bridge("sc_tgt_bridge"),
      sc_wrap_init("sc_wrap_init"),
      sc_wrap_tgt("sc_wrap_tgt"),
      sc_wrap_target_mod("sc_wrap_target_mod"),
      b_imp("b_imp", &b_imp_obj),
      imp_b_init_skt("imp_b_init_skt"),
      imp_b_tgt_skt("imp_b_tgt_skt"),
      nb_fw_imp("nb_fw_imp", &nb_imp_obj),
      nb_bw_imp("nb_bw_imp", &nb_imp_obj),
      nb_both_imp("nb_both_imp", &nb_imp_obj),
      imp_nb_init_skt("imp_nb_init_skt"),
      imp_nb_tgt_skt("imp_nb_tgt_skt"),
      imp_nb_init_both_skt("imp_nb_init_both_skt"),
      imp_nb_tgt_both_skt("imp_nb_tgt_both_skt"),
      sc_nb_init_bridge("sc_nb_init_bridge"),
      sc_nb_tgt("sc_nb_tgt"),
      sc_nb_init("sc_nb_init"),
      sc_nb_tgt_bridge("sc_nb_tgt_bridge"),
      sc_null_init("sc_null_init"),
      sc_null_tgt_bridge("sc_null_tgt_bridge"),
      sc_undersized_init("sc_undersized_init"),
      sc_undersized_tgt_bridge("sc_undersized_tgt_bridge"),
      sc_reent_init("sc_reent_init"),
      sc_reent_tgt_bridge("sc_reent_tgt_bridge"),
      sc_reent_init_bridge("sc_reent_init_bridge"),
      sc_reent_tgt("sc_reent_tgt") {}

  void check(bool cond, const std::string& msg)
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("TLM2_TEST", "FAILED: " + msg);
    }
  }

  virtual void build_phase(uvm_phase& phase)
  {
    uvm_test::build_phase(phase);
    b_init_w = b_init_wrapper::type_id::create("b_init_w", this);
    b_tgt_w = b_tgt_wrapper::type_id::create("b_tgt_w", this);
    nb_init_w = nb_init_wrapper::type_id::create("nb_init_w", this);
    nb_tgt_w = nb_tgt_wrapper::type_id::create("nb_tgt_w", this);
    uvm_tgt_from_sc = b_target_comp::type_id::create("uvm_tgt_from_sc", this);
    bridge_nb_init = bridge_nb_uvm_init::type_id::create("bridge_nb_init", this);
    uvm_nb_tgt_from_sc = nb_target_comp::type_id::create("uvm_nb_tgt_from_sc", this);
    uvm_null_tgt = uvm_null_ptr_target_comp::type_id::create("uvm_null_tgt", this);
    uvm_oversized_tgt = uvm_oversized_resp_target_comp::type_id::create("uvm_oversized_tgt", this);
    uvm_reent_tgt = reentrant_uvm_tgt_comp::type_id::create("uvm_reent_tgt", this);
    uvm_reent_init = reentrant_uvm_init_comp::type_id::create("uvm_reent_init", this);
  }

  virtual void connect_phase(uvm_phase& phase)
  {
    uvm_test::connect_phase(phase);
    b_init_w->pt_init.connect(b_tgt_w->pt_tgt);
    nb_init_w->pt_init.connect(nb_tgt_w->pt_tgt);

    uvm_to_sc_init.connect(sc_init_bridge.uvm_b_target_skt);
    sc_init_bridge.sc_init_skt.bind(sc_tgt.skt);

    sc_init.skt.bind(sc_tgt_bridge.sc_target_skt);
    sc_tgt_bridge.uvm_b_init_skt.connect(uvm_tgt_from_sc->target_skt);

    sc_wrap_init.connect(sc_wrap_bw);
    sc_wrap_tgt.connect(sc_wrap_target_mod.skt);
    sc_wrap_init.connect(sc_wrap_tgt);

    // Direct imp connections (no explicit cast)
    imp_b_tgt_skt.connect(b_imp);
    imp_b_init_skt.connect(imp_b_tgt_skt);

    imp_nb_init_skt.connect(nb_bw_imp);
    imp_nb_tgt_skt.connect(nb_fw_imp);
    imp_nb_init_skt.connect(imp_nb_tgt_skt);

    imp_nb_init_both_skt.connect(nb_both_imp);
    imp_nb_tgt_both_skt.connect(nb_both_imp);
    imp_nb_init_both_skt.connect(imp_nb_tgt_both_skt);

    // NB SystemC bridge connections
    bridge_nb_init->init_skt.connect(sc_nb_init_bridge.uvm_nb_target_skt);
    sc_nb_init_bridge.sc_init_skt.bind(sc_nb_tgt.skt);

    sc_nb_init.skt.bind(sc_nb_tgt_bridge.sc_target_skt);
    sc_nb_tgt_bridge.uvm_nb_init_skt.connect(uvm_nb_tgt_from_sc->target_skt);

    // Null-ptr UAF target bridge test connections
    sc_null_init.skt.bind(sc_null_tgt_bridge.sc_target_skt);
    sc_null_tgt_bridge.uvm_b_init_skt.connect(uvm_null_tgt->b_skt);
    sc_null_tgt_bridge.uvm_nb_init_skt.connect(uvm_null_tgt->nb_skt);

    // Undersized caller buffer target bridge test connections
    sc_undersized_init.skt.bind(sc_undersized_tgt_bridge.sc_target_skt);
    sc_undersized_tgt_bridge.uvm_b_init_skt.connect(uvm_oversized_tgt->b_skt);
    sc_undersized_tgt_bridge.uvm_nb_init_skt.connect(uvm_oversized_tgt->nb_skt);

    // Re-entrant NB bridge test connections (Section 7f)
    sc_reent_init.skt.bind(sc_reent_tgt_bridge.sc_target_skt);
    sc_reent_tgt_bridge.uvm_nb_init_skt.connect(uvm_reent_tgt->nb_skt);
    uvm_reent_init->nb_skt.connect(sc_reent_init_bridge.uvm_nb_target_skt);
    sc_reent_init_bridge.sc_init_skt.bind(sc_reent_tgt.skt);
  }

  virtual void run_phase(uvm_phase& phase)
  {
    phase.raise_objection(this);

    tlm2_msg_catcher catcher;
    uvm_report_cb::add(nullptr, &catcher);

    // 1. uvm_tlm_time tests
    {
      uvm_tlm_time t("t");
      check(std::fabs(t.get_resolution() - 1.0e-12) < 1e-24, "default res 1e-12");
      t.incr(5.0, 1.0e-9);
      check(std::fabs(t.get_realtime(1.0e-9) - 5.0) < 1e-9, "incr 5ns -> get_realtime(1e-9)");
      check(std::fabs(t.get_realtime() - 5.0e-9) < 1e-18, "get_realtime() in seconds");
      t.incr(sc_core::sc_time(3, sc_core::SC_NS));
      check(std::fabs(t.get_realtime(1.0e-9) - 8.0) < 1e-9, "incr sc_time(3ns)");
      t.decr(2.0, 1.0e-9);
      check(std::fabs(t.get_realtime(1.0e-9) - 6.0) < 1e-9, "decr 2ns");
      check(t.to_sc_time() == sc_core::sc_time(6, sc_core::SC_NS), "to_sc_time == 6ns");

      t.set_abstime(12.5, 1.0e-9);
      check(std::fabs(t.get_abstime(1.0e-9) - 12.5) < 1e-9, "set/get_abstime");
      t.from_sc_time(sc_core::sc_time(4, sc_core::SC_NS));
      check(std::fabs(t.get_realtime(1.0e-9) - 4.0) < 1e-9, "from_sc_time");
      t.reset();
      check(t.get_realtime() == 0.0, "reset zeroes time");

      int err_before = catcher.error_count;
      uvm_tlm_time bad_res("bad_res", 0.0);
      uvm_tlm_time neg_res("neg_res", -1.0e-9);
      t.incr(-1.0, 1.0e-9);
      t.decr(-1.0, 1.0e-9);
      t.incr(2.0, 1.0e-9);
      t.decr(5.0, 1.0e-9); // underflow
      check(catcher.error_count == err_before + 5, "uvm_tlm_time catches 5 error cases");
      check(t.get_realtime() == 0.0, "underflow clamps time to 0");

      // Large time small underflow detection & sub-1e-24 equality precision
      uvm_tlm_time t_big("t_big", 1.0e-12);
      t_big.incr(1000.0);
      t_big.decr(1000.0);
      check(catcher.error_count == err_before + 5 && t_big.get_realtime() == 0.0,
            "1000.0s decr by exact 1000.0s does not emit NEGTIME");
      t_big.incr(1000.0);
      t_big.decr(1000.0 + 0.5e-9); // 0.5ns underflow must be caught
      check(catcher.error_count == err_before + 6 && t_big.get_realtime() == 0.0,
            "1000.0s decr by 1000.0s+0.5ns triggers UVM/TLM/NEGTIME");

      uvm_tlm_time ts1("ts1", 1.0e-25);
      ts1.incr(1.0e-25);
      uvm_tlm_time ts0("ts0", 1.0e-25);
      check(ts1 != ts0, "1e-25s != 0s");
    }

    // 2. uvm_tlm_gp & uvm_tlm_extension<T> tests
    {
      uvm_tlm_gp* gp1 = uvm_tlm_gp::type_id::create("gp1");
      check(gp1 != nullptr, "uvm_tlm_gp factory create");
      gp1->set_write();
      check(gp1->is_write() && !gp1->is_read(), "is_write");
      gp1->set_address(0x10203040ULL);
      std::vector<unsigned char> wdata = {0xDE, 0xAD, 0xBE, 0xEF};
      std::vector<unsigned char> wbe = {UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED,
                                        UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_ENABLED};
      gp1->set_data_length(4);
      gp1->set_data(wdata);
      gp1->set_byte_enable_length(4);
      gp1->set_byte_enable(wbe);
      gp1->set_streaming_width(4);
      gp1->set_dmi_allowed(true);
      gp1->set_gp_option(UVM_TLM_FULL_PAYLOAD);
      gp1->set_response_status(UVM_TLM_OK_RESPONSE);
      check(gp1->is_response_ok() && !gp1->is_response_error(), "is_response_ok");
      check(gp1->get_response_string() == "UVM_TLM_OK_RESPONSE", "get_response_string");

      my_bus_ext* ext1 = new my_bus_ext("ext1");
      ext1->domain_id = 7;
      ext1->secure = true;
      uvm_tlm_extension_base* prev = gp1->set_extension(ext1);
      check(prev == nullptr, "first set_extension returns nullptr");
      check(gp1->get_num_extensions() == 1, "num_extensions == 1");
      my_bus_ext* got_ext = nullptr;
      gp1->get_extension(got_ext);
      check(got_ext == ext1 && got_ext->domain_id == 7 && got_ext->secure, "get_extension");

      // Clone + deep copy (also exercises owned extension cleanup on destroy)
      uvm_object* cl_obj = gp1->clone();
      uvm_tlm_gp* gp2 = dynamic_cast<uvm_tlm_gp*>(cl_obj);
      check(gp2 != nullptr, "clone() returns uvm_tlm_gp");
      check(gp2->compare(*gp1), "clone compares equal to original");

      // Mutate clone data & extension, ensure original unaffected
      gp2->m_data[0] = 0x11;
      my_bus_ext* gp2_ext = gp2->get_extension<my_bus_ext>();
      check(gp2_ext != nullptr && gp2_ext != ext1, "clone deep-copied extension");
      gp2_ext->domain_id = 99;
      check(gp1->m_data[0] == 0xDE && ext1->domain_id == 7, "deep copy isolation");
      check(!gp2->compare(*gp1), "mutated clone compares unequal");

      // Direct do_copy self-copy must preserve fields and extensions
      gp1->do_copy(*gp1);
      check(gp1->get_num_extensions() == 1 &&
            gp1->get_extension<my_bus_ext>() == ext1 &&
            gp1->m_data == wdata,
            "direct do_copy self-copy preserves extensions and data");

      // do_compare edge cases: byte_enable buffer presence & short-buffer asymmetry
      {
        uvm_comparer quiet_cmp("quiet_cmp");
        quiet_cmp.show_max = 0;

        uvm_tlm_gp be_a("be_a"), be_c("be_c");
        be_a.set_byte_enable_length(4);
        be_a.set_byte_enable({0xff, 0x00, 0xff, 0x00});
        be_c.set_byte_enable_length(4);
        check(!be_a.compare(be_c, &quiet_cmp),
              "do_compare detects byte_enable_length=4 populated vs empty");

        uvm_tlm_gp sb1("sb1"), sb2("sb2"), sb3("sb3");
        sb1.set_data_length(4);
        sb1.set_data({0x01, 0x02});
        sb2.set_data_length(4);
        sb2.set_data({0x01, 0x02, 0x03});
        sb3.set_data_length(4);
        sb3.set_data({0x01, 0x02});
        check(!sb1.compare(sb2, &quiet_cmp),
              "do_compare detects short-buffer m_data size asymmetry");
        check(sb1.compare(sb3, &quiet_cmp),
              "do_compare matches identical short m_data buffers");

        uvm_tlm_gp sbe1("sbe1"), sbe2("sbe2");
        sbe1.set_byte_enable_length(4);
        sbe1.set_byte_enable({0xff, 0x00});
        sbe2.set_byte_enable_length(4);
        sbe2.set_byte_enable({0xff, 0x00, 0xff});
        check(!sbe1.compare(sbe2, &quiet_cmp),
              "do_compare detects short-buffer m_byte_enable size asymmetry");
      }

      // Pack & unpack (bits, bytes, ints, big_endian)
      gp2->copy(*gp1);
      gp2->copy(*gp1); // re-copy over GP with owned cloned extension (leak check)
      std::vector<bool> packed_bits;
      std::vector<unsigned char> packed_bytes;
      std::vector<unsigned int> packed_ints;
      int nbits = gp1->pack(packed_bits);
      check(nbits > 0 && !packed_bits.empty(), "pack(bits)");
      uvm_tlm_gp gp_bits("gp_bits");
      gp_bits.unpack(packed_bits);
      check(gp_bits.get_address() == gp1->get_address() &&
            gp_bits.m_data == wdata &&
            gp_bits.m_byte_enable == wbe,
            "pack/unpack bits round-trip");

      nbits = gp1->pack_bytes(packed_bytes);
      check(nbits > 0 && !packed_bytes.empty(), "pack_bytes");
      uvm_tlm_gp gp3("gp3");
      gp3.unpack_bytes(packed_bytes);
      check(gp3.get_address() == gp1->get_address() &&
            gp3.get_command() == gp1->get_command() &&
            gp3.get_data_length() == 4 &&
            gp3.m_data == wdata &&
            gp3.m_byte_enable == wbe &&
            gp3.is_dmi_allowed() == true &&
            gp3.get_response_status() == UVM_TLM_OK_RESPONSE,
            "pack_bytes/unpack_bytes round-trip");

      uvm_packer be_p("be_p");
      be_p.big_endian = true;
      gp1->pack_ints(packed_ints, &be_p);
      uvm_tlm_gp gp_ints("gp_ints");
      gp_ints.unpack_ints(packed_ints, &be_p);
      check(gp_ints.get_address() == gp1->get_address() &&
            gp_ints.m_data == wdata &&
            gp_ints.get_response_status() == UVM_TLM_OK_RESPONSE,
            "pack_ints/unpack_ints big_endian round-trip");

      // Print (table, JSON, XML) & convert2string
      std::string tbl_s = gp1->sprint();
      uvm_json_printer jp("jp");
      std::string json_s = gp1->sprint(&jp);
      uvm_xml_printer xp("xp");
      std::string xml_s = gp1->sprint(&xp);
      std::string c2s = gp1->convert2string();
      check(!tbl_s.empty() && !json_s.empty() && !xml_s.empty() && !c2s.empty(),
            "print/sprint/convert2string non-empty");
      check(json_s.find("\"address\"") != std::string::npos, "JSON contains address");
      check(xml_s.find("<object") != std::string::npos, "XML contains <object");

      // Record & verify recorded output
      uvm_text_tr_database db("tlm2_db");
      db.set_file_name("uvm_tlm2_test_tr.log");
      if (db.open_db())
      {
        uvm_tr_stream* strm = db.open_stream("gp_strm");
        uvm_recorder* rec = strm->open_recorder("gp_rec");
        gp1->record(rec);
        rec->close();
        rec->free();
        db.close_db();
      }
      {
        std::ifstream tr_ifs("uvm_tlm2_test_tr.log");
        std::ostringstream tr_buf;
        tr_buf << tr_ifs.rdbuf();
        const std::string tr_log = tr_buf.str();
        check(tr_log.find("address") != std::string::npos &&
              tr_log.find("command") != std::string::npos &&
              tr_log.find("data_length") != std::string::npos &&
              tr_log.find("data[0]") != std::string::npos &&
              tr_log.find("response_status") != std::string::npos &&
              tr_log.find("byte_enable_length") != std::string::npos &&
              tr_log.find("streaming_width") != std::string::npos &&
              tr_log.find("gp_option") != std::string::npos,
              "do_record writes all GP attributes to tr database");
      }

      // SystemC tlm::tlm_generic_payload round-trip & clamped buffer copy check
      {
        uvm_tlm_gp gp_ov("gp_ov");
        gp_ov.set_data_length(2);
        gp_ov.set_data({0x11, 0x22, 0x33, 0x44});
        gp_ov.set_byte_enable_length(1);
        gp_ov.set_byte_enable({0xff, 0x00, 0xff});
        tlm::tlm_generic_payload sc_ov;
        std::unique_ptr<unsigned char[]> exact_data_buf(new unsigned char[2]());
        std::unique_ptr<unsigned char[]> exact_be_buf(new unsigned char[1]());
        gp_ov.to_tlm_gp(sc_ov, exact_data_buf.get(), exact_be_buf.get());
        check(exact_data_buf[0] == 0x11 && exact_data_buf[1] == 0x22 &&
              exact_be_buf[0] == 0xff,
              "to_tlm_gp clamps memcpy to m_length / m_byte_enable_length");
      }

      // SystemC tlm::tlm_generic_payload round-trip
      tlm::tlm_generic_payload sc_gp;
      std::vector<unsigned char> sc_data(4, 0), sc_be(4, 0);
      gp1->to_tlm_gp(sc_gp, sc_data.data(), sc_be.data());
      check(sc_gp.get_command() == tlm::TLM_WRITE_COMMAND &&
            sc_gp.get_address() == 0x10203040ULL &&
            sc_gp.get_data_length() == 4 &&
            sc_data == wdata &&
            sc_be == wbe,
            "to_tlm_gp conversion");
      sc_data[1] = 0x55;
      uvm_tlm_gp gp4("gp4");
      gp4.from_tlm_gp(sc_gp);
      check(gp4.m_data[1] == 0x55 && gp4.get_address() == 0x10203040ULL,
            "from_tlm_gp conversion");

      gp1->clear_extension<my_bus_ext>();
      check(gp1->get_num_extensions() == 0, "clear_extension");
      delete ext1;
      delete gp2;
      uvm_tlm_gp::type_id::destroy(gp1);
    }

    // 3. Blocking UVM TLM-2 sockets (passthrough hierarchy + unbound fatal)
    {
      uvm_tlm_gp tx("tx");
      tx.set_write();
      tx.set_address(0x10);
      tx.set_data_length(4);
      tx.set_data({0x11, 0x22, 0x33, 0x44});
      uvm_tlm_time d("d");
      b_init_w->child->init_skt.b_transport(tx, d);
      check(tx.is_response_ok(), "b_transport write ok");
      check(std::fabs(d.get_realtime(1.0e-9) - 5.0) < 1e-9, "b_transport delay incremented");

      uvm_tlm_gp rx("rx");
      rx.set_read();
      rx.set_address(0x10);
      rx.set_data_length(4);
      rx.set_data({0, 0, 0, 0});
      uvm_tlm_time d2("d2");
      b_init_w->child->init_skt.b_transport(rx, d2);
      check(rx.is_response_ok() &&
            rx.m_data == std::vector<unsigned char>({0x11, 0x22, 0x33, 0x44}),
            "b_transport read back data");

      int fat_before = catcher.fatal_count;
      unbound_b_init.b_transport(rx, d2);
      check(catcher.fatal_count == fat_before + 1, "unbound b_transport emits UVM_FATAL");
    }

    // 4. Non-blocking UVM TLM-2 sockets (4-phase + early completed)
    {
      uvm_tlm_gp nb_tx("nb_tx");
      nb_tx.set_write();
      nb_tx.set_address(0x20);
      nb_tx.set_data_length(2);
      nb_tx.set_data({0xAA, 0xBB});
      uvm_tlm_phase_e ph = BEGIN_REQ;
      uvm_tlm_time d("d");
      uvm_tlm_sync_e sync = nb_init_w->child->init_skt.nb_transport_fw(nb_tx, ph, d);
      check(sync == UVM_TLM_ACCEPTED, "nb_transport_fw BEGIN_REQ returns UVM_TLM_ACCEPTED");
      sc_core::wait(nb_init_w->child->resp_ev);
      check(nb_init_w->child->got_end_req && nb_init_w->child->got_begin_resp &&
            nb_tx.is_response_ok(),
            "nb_transport 4-phase END_REQ + BEGIN_RESP completed");

      nb_tgt_w->child->early_complete = true;
      uvm_tlm_gp nb_tx2("nb_tx2");
      nb_tx2.set_read();
      ph = BEGIN_REQ;
      sync = nb_init_w->child->init_skt.nb_transport_fw(nb_tx2, ph, d);
      check(sync == UVM_TLM_COMPLETED && ph == END_RESP && nb_tx2.is_response_ok(),
            "nb_transport early UVM_TLM_COMPLETED");
    }

    // 5. SystemC 3.0.3 TLM-2 interop (bridges & sc socket wrappers)
    {
      // UVM initiator -> bridge -> SystemC simple_target_socket
      uvm_tlm_gp gp("u2sc");
      gp.set_write();
      gp.set_address(0x04);
      gp.set_data_length(4);
      gp.set_data({0xCA, 0xFE, 0xBA, 0xBE});
      uvm_tlm_time d("d");
      uvm_to_sc_init.b_transport(gp, d);
      check(gp.is_response_ok() &&
            sc_tgt.mem[4] == 0xCA && sc_tgt.mem[7] == 0xBE,
            "UVM -> SystemC target bridge write");

      gp.set_read();
      gp.set_data({0, 0, 0, 0});
      uvm_to_sc_init.b_transport(gp, d);
      check(gp.is_response_ok() &&
            gp.m_data == std::vector<unsigned char>({0xCA, 0xFE, 0xBA, 0xBE}),
            "UVM -> SystemC target bridge read");

      // READ across initiator bridge when m_data is NOT pre-sized (empty)
      uvm_tlm_gp gp_empty_rd("gp_empty_rd");
      gp_empty_rd.set_read();
      gp_empty_rd.set_address(0x04);
      gp_empty_rd.set_data_length(4);
      uvm_tlm_time d_rd("d_rd");
      uvm_to_sc_init.b_transport(gp_empty_rd, d_rd);
      check(gp_empty_rd.is_response_ok() &&
            gp_empty_rd.m_data == std::vector<unsigned char>({0xCA, 0xFE, 0xBA, 0xBE}),
            "UVM -> SystemC initiator bridge b_transport READ with empty m_data");

      // SystemC initiator -> bridge -> UVM target
      tlm::tlm_generic_payload sc_gp;
      unsigned char wbuf[4] = {0x12, 0x34, 0x56, 0x78};
      sc_gp.set_command(tlm::TLM_WRITE_COMMAND);
      sc_gp.set_address(0x08);
      sc_gp.set_data_ptr(wbuf);
      sc_gp.set_data_length(4);
      sc_gp.set_streaming_width(4);
      sc_gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
      sc_core::sc_time sc_d = sc_core::SC_ZERO_TIME;
      sc_init.skt->b_transport(sc_gp, sc_d);
      check(sc_gp.is_response_ok() &&
            uvm_tgt_from_sc->mem[8] == 0x12 && uvm_tgt_from_sc->mem[11] == 0x78,
            "SystemC -> UVM target bridge write");

      // sc_wrap_init -> sc_wrap_tgt -> SystemC simple_target_socket
      unsigned char wbuf2[2] = {0x9A, 0xBC};
      sc_gp.set_command(tlm::TLM_WRITE_COMMAND);
      sc_gp.set_address(0x0C);
      sc_gp.set_data_ptr(wbuf2);
      sc_gp.set_data_length(2);
      sc_gp.set_streaming_width(2);
      sc_gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
      sc_wrap_init->b_transport(sc_gp, sc_d);
      check(sc_gp.is_response_ok() &&
            sc_wrap_target_mod.mem[0x0C] == 0x9A && sc_wrap_target_mod.mem[0x0D] == 0xBC,
            "uvm_tlm_sc_initiator_socket/target_socket interop");
    }

    // 6. Direct uvm_tlm_*_imp connections (no explicit cast)
    {
      uvm_tlm_gp gp("imp_b_gp");
      gp.set_read();
      gp.set_address(0x40);
      gp.set_data_length(4);
      uvm_tlm_time d("imp_b_d");
      imp_b_init_skt.b_transport(gp, d);
      check(gp.is_response_ok() && b_imp_obj.calls == 1 && b_imp_obj.last_addr == 0x40 &&
            gp.m_data == std::vector<unsigned char>({0xA1, 0xB2, 0xC3, 0xD4}),
            "uvm_tlm_b_target_socket::connect(uvm_tlm_b_transport_imp) without cast");

      uvm_tlm_gp nb_gp1("imp_nb_gp1");
      nb_gp1.set_write();
      uvm_tlm_phase_e ph = BEGIN_REQ;
      uvm_tlm_time nd("imp_nb_d");
      uvm_tlm_sync_e sync = imp_nb_init_skt.nb_transport_fw(nb_gp1, ph, nd);
      check(sync == UVM_TLM_COMPLETED && ph == END_RESP && nb_imp_obj.fw_calls == 1,
            "uvm_tlm_nb_transport_fw_imp direct connect");

      uvm_tlm_gp nb_gp2("imp_nb_gp2");
      ph = BEGIN_RESP;
      sync = imp_nb_tgt_skt.nb_transport_bw(nb_gp2, ph, nd);
      check(sync == UVM_TLM_COMPLETED && nb_imp_obj.bw_calls == 1,
            "uvm_tlm_nb_transport_bw_imp direct connect");

      uvm_tlm_gp nb_gp3("imp_nb_gp3");
      nb_gp3.set_write();
      ph = BEGIN_REQ;
      sync = imp_nb_init_both_skt.nb_transport_fw(nb_gp3, ph, nd);
      check(sync == UVM_TLM_COMPLETED && nb_imp_obj.fw_calls == 2,
            "uvm_tlm_nb_transport_imp direct connect fw");
      ph = BEGIN_RESP;
      sync = imp_nb_tgt_both_skt.nb_transport_bw(nb_gp3, ph, nd);
      check(sync == UVM_TLM_COMPLETED && nb_imp_obj.bw_calls == 2,
            "uvm_tlm_nb_transport_imp direct connect bw");
    }

    // 7. NB SystemC bridges & target_bridge null-ptr UAF regression tests
    {
      // 7a. UVM NB initiator -> bridge -> SystemC NB target (write then empty-m_data READ)
      uvm_tlm_gp nb_w("nb_w");
      nb_w.set_write();
      nb_w.set_address(0x10);
      nb_w.set_data_length(4);
      nb_w.set_data({0x55, 0x66, 0x77, 0x88});
      uvm_tlm_phase_e ph = BEGIN_REQ;
      uvm_tlm_time d("nb_bd");
      bridge_nb_init->got_end_req = false;
      bridge_nb_init->got_begin_resp = false;
      uvm_tlm_sync_e sync = bridge_nb_init->init_skt.nb_transport_fw(nb_w, ph, d);
      check(sync == UVM_TLM_ACCEPTED, "init_bridge nb_transport_fw WRITE accepted");
      sc_core::wait(bridge_nb_init->done_ev);
      check(bridge_nb_init->got_end_req && bridge_nb_init->got_begin_resp &&
            nb_w.is_response_ok() && sc_nb_tgt.mem[0x10] == 0x55 && sc_nb_tgt.mem[0x13] == 0x88,
            "init_bridge 4-phase NB WRITE completed");

      uvm_tlm_gp nb_r("nb_r");
      nb_r.set_read();
      nb_r.set_address(0x10);
      nb_r.set_data_length(4);
      ph = BEGIN_REQ;
      bridge_nb_init->got_end_req = false;
      bridge_nb_init->got_begin_resp = false;
      sync = bridge_nb_init->init_skt.nb_transport_fw(nb_r, ph, d);
      check(sync == UVM_TLM_ACCEPTED, "init_bridge nb_transport_fw empty-m_data READ accepted");
      sc_core::wait(bridge_nb_init->done_ev);
      check(bridge_nb_init->got_end_req && bridge_nb_init->got_begin_resp &&
            !sc_nb_tgt.saw_null_ptr && nb_r.is_response_ok() &&
            nb_r.m_data == std::vector<unsigned char>({0x55, 0x66, 0x77, 0x88}),
            "init_bridge 4-phase NB READ with empty m_data completed");

      // Transition m_data from non-empty to empty across NB phases
      uvm_tlm_gp nb_clr("nb_clr");
      nb_clr.set_write();
      nb_clr.set_address(0x34);
      nb_clr.set_data_length(2);
      nb_clr.set_data({0xAA, 0xBB});
      ph = BEGIN_REQ;
      bridge_nb_init->got_end_req = false;
      bridge_nb_init->got_begin_resp = false;
      sync = bridge_nb_init->init_skt.nb_transport_fw(nb_clr, ph, d);
      check(sync == UVM_TLM_ACCEPTED, "init_bridge nb_transport_fw transition accepted");
      sc_core::wait(bridge_nb_init->done_ev);
      check(bridge_nb_init->got_end_req && bridge_nb_init->got_begin_resp,
            "init_bridge nb_transport m_data non-empty->empty transition safe");

      // 7b. SystemC NB initiator -> bridge -> UVM NB target (4-phase)
      tlm::tlm_generic_payload sc_nb_gp;
      unsigned char nb_buf[4] = {0x41, 0x42, 0x43, 0x44};
      sc_nb_gp.set_command(tlm::TLM_WRITE_COMMAND);
      sc_nb_gp.set_address(0x18);
      sc_nb_gp.set_data_ptr(nb_buf);
      sc_nb_gp.set_data_length(4);
      sc_nb_gp.set_streaming_width(4);
      sc_nb_gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
      tlm::tlm_phase sc_ph = tlm::BEGIN_REQ;
      sc_core::sc_time sc_nb_d = sc_core::SC_ZERO_TIME;
      uvm_nb_tgt_from_sc->early_complete = false;
      tlm::tlm_sync_enum sc_sync = sc_nb_init.skt->nb_transport_fw(sc_nb_gp, sc_ph, sc_nb_d);
      check(sc_sync == tlm::TLM_ACCEPTED, "tgt_bridge nb_transport_fw accepted");
      sc_core::wait(sc_nb_init.done_ev);
      check(sc_nb_init.got_end_req && sc_nb_init.got_begin_resp && sc_nb_gp.is_response_ok(),
            "tgt_bridge 4-phase NB completed");

      // 7c. SystemC initiator with nullptr data_ptr & byte_enable_ptr -> target_bridge -> UVM target
      tlm::tlm_generic_payload sc_null_b;
      sc_null_b.set_command(tlm::TLM_READ_COMMAND);
      sc_null_b.set_address(0x00);
      sc_null_b.set_data_ptr(nullptr);
      sc_null_b.set_data_length(4);
      sc_null_b.set_byte_enable_ptr(nullptr);
      sc_null_b.set_byte_enable_length(2);
      sc_core::sc_time sc_null_d = sc_core::SC_ZERO_TIME;
      sc_null_init.skt->b_transport(sc_null_b, sc_null_d);
      check(sc_null_b.is_response_ok() &&
            sc_null_b.get_data_ptr() == nullptr &&
            sc_null_b.get_byte_enable_ptr() == nullptr,
            "tgt_bridge b_transport preserves nullptr data_ptr/be_ptr without UAF");

      tlm::tlm_generic_payload sc_null_nb;
      sc_null_nb.set_command(tlm::TLM_READ_COMMAND);
      sc_null_nb.set_address(0x00);
      sc_null_nb.set_data_ptr(nullptr);
      sc_null_nb.set_data_length(4);
      sc_null_nb.set_byte_enable_ptr(nullptr);
      sc_null_nb.set_byte_enable_length(2);
      tlm::tlm_phase sc_null_ph = tlm::BEGIN_REQ;
      tlm::tlm_sync_enum sc_null_sync =
        sc_null_init.skt->nb_transport_fw(sc_null_nb, sc_null_ph, sc_null_d);
      check(sc_null_sync == tlm::TLM_COMPLETED &&
            sc_null_nb.is_response_ok() &&
            sc_null_nb.get_data_ptr() == nullptr &&
            sc_null_nb.get_byte_enable_ptr() == nullptr,
            "tgt_bridge nb_transport_fw early COMPLETED preserves nullptr data_ptr/be_ptr without UAF");

      // 7d. Undersized caller buffers (2-byte data_ptr, 1-byte be_ptr) with oversized UVM target response
      {
        std::unique_ptr<unsigned char[]> small_data(new unsigned char[2]());
        std::unique_ptr<unsigned char[]> small_be(new unsigned char[1]());
        tlm::tlm_generic_payload sc_small_b;
        sc_small_b.set_command(tlm::TLM_READ_COMMAND);
        sc_small_b.set_address(0x00);
        sc_small_b.set_data_ptr(small_data.get());
        sc_small_b.set_data_length(2);
        sc_small_b.set_byte_enable_ptr(small_be.get());
        sc_small_b.set_byte_enable_length(1);
        sc_core::sc_time sc_small_d = sc_core::SC_ZERO_TIME;
        sc_undersized_init.skt->b_transport(sc_small_b, sc_small_d);
        check(sc_small_b.is_response_ok() &&
              sc_small_b.get_data_length() <= 2 &&
              sc_small_b.get_byte_enable_length() <= 1 &&
              small_data[0] == 0x10 && small_data[1] == 0x20 &&
              small_be[0] == UVM_TLM_BYTE_ENABLED,
              "tgt_bridge b_transport clamps data_length/byte_enable_length to caller buffer capacity");

        std::unique_ptr<unsigned char[]> nb_small_data(new unsigned char[2]());
        std::unique_ptr<unsigned char[]> nb_small_be(new unsigned char[1]());
        tlm::tlm_generic_payload sc_small_nb;
        sc_small_nb.set_command(tlm::TLM_READ_COMMAND);
        sc_small_nb.set_address(0x00);
        sc_small_nb.set_data_ptr(nb_small_data.get());
        sc_small_nb.set_data_length(2);
        sc_small_nb.set_byte_enable_ptr(nb_small_be.get());
        sc_small_nb.set_byte_enable_length(1);
        tlm::tlm_phase sc_small_ph = tlm::BEGIN_REQ;
        sc_undersized_init.got_end_req = false;
        sc_undersized_init.got_begin_resp = false;
        tlm::tlm_sync_enum sc_small_sync =
          sc_undersized_init.skt->nb_transport_fw(sc_small_nb, sc_small_ph, sc_small_d);
        check(sc_small_sync == tlm::TLM_ACCEPTED, "tgt_bridge undersized nb_transport_fw accepted");
        sc_core::wait(sc_undersized_init.done_ev);
        check(sc_undersized_init.got_end_req && sc_undersized_init.got_begin_resp &&
              sc_small_nb.is_response_ok() &&
              sc_small_nb.get_data_length() <= 2 &&
              sc_small_nb.get_byte_enable_length() <= 1 &&
              nb_small_data[0] == 0xA0 && nb_small_data[1] == 0xB0 &&
              nb_small_be[0] == UVM_TLM_BYTE_ENABLED,
              "tgt_bridge 4-phase nb_transport_bw clamps lengths and avoids OOB read");
      }

      // 7e. Short m_byte_enable (size 2 < byte_enable_length 4) across initiator bridge
      {
        uvm_tlm_gp gp_short_be("gp_short_be");
        gp_short_be.set_write();
        gp_short_be.set_address(0x20);
        gp_short_be.set_data_length(4);
        gp_short_be.set_data({0x11, 0x22, 0x33, 0x44});
        gp_short_be.set_byte_enable_length(4);
        gp_short_be.set_byte_enable({UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED});
        uvm_tlm_time d_be("d_be");
        uvm_to_sc_init.b_transport(gp_short_be, d_be);
        check(gp_short_be.is_response_ok() &&
              sc_tgt.mem[0x20] == 0x11 && sc_tgt.mem[0x21] == 0x00 &&
              sc_tgt.mem[0x22] == 0x00 && sc_tgt.mem[0x23] == 0x00,
              "init_bridge b_transport pads short be_buf to byte_enable_length");

        // Empty m_byte_enable with non-zero m_byte_enable_length preserves be_ptr==nullptr => be_len==0
        uvm_tlm_gp gp_empty_be("gp_empty_be");
        gp_empty_be.set_write();
        gp_empty_be.set_address(0x24);
        gp_empty_be.set_data_length(2);
        gp_empty_be.set_data({0x5A, 0xA5});
        gp_empty_be.set_byte_enable_length(4);
        uvm_to_sc_init.b_transport(gp_empty_be, d_be);
        check(gp_empty_be.is_response_ok() &&
              sc_tgt.mem[0x24] == 0x5A && sc_tgt.mem[0x25] == 0xA5 &&
              gp_empty_be.get_byte_enable_length() == 0,
              "init_bridge b_transport resets byte_enable_length=0 when m_byte_enable is empty");

        uvm_tlm_gp nb_short_be("nb_short_be");
        nb_short_be.set_write();
        nb_short_be.set_address(0x28);
        nb_short_be.set_data_length(4);
        nb_short_be.set_data({0xAA, 0xBB, 0xCC, 0xDD});
        nb_short_be.set_byte_enable_length(4);
        nb_short_be.set_byte_enable({UVM_TLM_BYTE_ENABLED, UVM_TLM_BYTE_DISABLED});
        uvm_tlm_phase_e ph_be = BEGIN_REQ;
        bridge_nb_init->got_end_req = false;
        bridge_nb_init->got_begin_resp = false;
        uvm_tlm_sync_e sync_be = bridge_nb_init->init_skt.nb_transport_fw(nb_short_be, ph_be, d_be);
        check(sync_be == UVM_TLM_ACCEPTED, "init_bridge nb_transport_fw short be accepted");
        sc_core::wait(bridge_nb_init->done_ev);
        check(bridge_nb_init->got_end_req && bridge_nb_init->got_begin_resp &&
              nb_short_be.is_response_ok() &&
              sc_nb_tgt.mem[0x28] == 0xAA && sc_nb_tgt.mem[0x29] == 0x00 &&
              sc_nb_tgt.mem[0x2A] == 0x00 && sc_nb_tgt.mem[0x2B] == 0x00,
              "init_bridge nb_transport_fw pads short be_buf to byte_enable_length");
      }

      // 7f. Synchronous re-entrant nb_transport_fw <-> nb_transport_bw completion (all 4 paths)
      {
        // 7f.1 target_bridge::nb_transport_fw -> sync nb_transport_bw -> TLM_COMPLETED
        sc_reent_init.call_end_resp_in_bw = false;
        sc_reent_init.bw_calls = 0;
        sc_reent_init.saw_ok = false;
        sc_reent_init.rx_data.clear();
        uvm_reent_tgt->sync_bw_in_fw = true;

        unsigned char rdata1[4] = {0, 0, 0, 0};
        tlm::tlm_generic_payload sc_gp1;
        sc_gp1.set_command(tlm::TLM_READ_COMMAND);
        sc_gp1.set_address(0x50);
        sc_gp1.set_data_ptr(rdata1);
        sc_gp1.set_data_length(4);
        sc_gp1.set_streaming_width(4);
        sc_gp1.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        tlm::tlm_phase ph1 = tlm::BEGIN_REQ;
        sc_core::sc_time d1 = sc_core::SC_ZERO_TIME;
        tlm::tlm_sync_enum sync1 = sc_reent_init.skt->nb_transport_fw(sc_gp1, ph1, d1);
        check(sync1 == tlm::TLM_ACCEPTED &&
              uvm_reent_tgt->bw_ret_sync == UVM_TLM_COMPLETED &&
              sc_reent_init.bw_calls == 1 && sc_reent_init.saw_ok &&
              sc_reent_init.rx_data == std::vector<unsigned char>({0xC1, 0xC2, 0xC3, 0xC4}) &&
              sc_gp1.is_response_ok() &&
              rdata1[0] == 0xC1 && rdata1[3] == 0xC4,
              "7f.1 target_bridge nb_transport_fw -> sync nb_transport_bw -> TLM_COMPLETED");

        // Reuse sc_gp1 after re-entrant completion
        std::memset(rdata1, 0, sizeof(rdata1));
        sc_gp1.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        ph1 = tlm::BEGIN_REQ;
        sync1 = sc_reent_init.skt->nb_transport_fw(sc_gp1, ph1, d1);
        check(sync1 == tlm::TLM_ACCEPTED && sc_reent_init.bw_calls == 2 && sc_gp1.is_response_ok(),
              "7f.1b target_bridge reused sc_gp after re-entrant completion");

        // 7f.2 initiator_bridge::nb_transport_fw -> sync nb_transport_bw -> UVM_TLM_COMPLETED
        uvm_reent_init->call_end_resp_in_bw = false;
        uvm_reent_init->bw_calls = 0;
        uvm_reent_init->saw_ok = false;
        uvm_reent_init->rx_data.clear();
        sc_reent_tgt.sync_bw_in_fw = true;

        uvm_tlm_gp uvm_gp2("uvm_gp2");
        uvm_gp2.set_read();
        uvm_gp2.set_address(0x54);
        uvm_gp2.set_data_length(4);
        uvm_tlm_phase_e ph2 = BEGIN_REQ;
        uvm_tlm_time d2("d2");
        uvm_tlm_sync_e sync2 = uvm_reent_init->nb_skt.nb_transport_fw(uvm_gp2, ph2, d2);
        check(sync2 == UVM_TLM_ACCEPTED &&
              sc_reent_tgt.bw_ret_sync == tlm::TLM_COMPLETED &&
              uvm_reent_init->bw_calls == 1 && uvm_reent_init->saw_ok &&
              uvm_reent_init->rx_data == std::vector<unsigned char>({0xE1, 0xE2, 0xE3, 0xE4}) &&
              uvm_gp2.is_response_ok() &&
              uvm_gp2.m_data == std::vector<unsigned char>({0xE1, 0xE2, 0xE3, 0xE4}),
              "7f.2 initiator_bridge nb_transport_fw -> sync nb_transport_bw -> UVM_TLM_COMPLETED");

        // Reuse uvm_gp2 after re-entrant completion
        uvm_gp2.set_response_status(UVM_TLM_INCOMPLETE_RESPONSE);
        uvm_gp2.m_data.clear();
        ph2 = BEGIN_REQ;
        sync2 = uvm_reent_init->nb_skt.nb_transport_fw(uvm_gp2, ph2, d2);
        check(sync2 == UVM_TLM_ACCEPTED && uvm_reent_init->bw_calls == 2 && uvm_gp2.is_response_ok(),
              "7f.2b initiator_bridge reused uvm_gp after re-entrant completion");

        // 7f.3 target_bridge::nb_transport_bw -> sync nb_transport_fw(END_RESP) -> UVM_TLM_COMPLETED
        sc_reent_init.call_end_resp_in_bw = true;
        sc_reent_init.bw_calls = 0;
        sc_reent_init.saw_ok = false;
        sc_reent_init.rx_data.clear();
        sc_reent_init.end_resp_fw_sync = tlm::TLM_ACCEPTED;
        uvm_reent_tgt->sync_bw_in_fw = false;
        uvm_reent_tgt->end_resp_seen = false;

        unsigned char rdata3[4] = {0, 0, 0, 0};
        tlm::tlm_generic_payload sc_gp3;
        sc_gp3.set_command(tlm::TLM_READ_COMMAND);
        sc_gp3.set_address(0x58);
        sc_gp3.set_data_ptr(rdata3);
        sc_gp3.set_data_length(4);
        sc_gp3.set_streaming_width(4);
        sc_gp3.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        tlm::tlm_phase ph3 = tlm::BEGIN_REQ;
        sc_core::sc_time d3 = sc_core::SC_ZERO_TIME;
        tlm::tlm_sync_enum sync3 = sc_reent_init.skt->nb_transport_fw(sc_gp3, ph3, d3);
        check(sync3 == tlm::TLM_ACCEPTED, "7f.3 target_bridge initial BEGIN_REQ accepted");
        sc_core::wait(sc_reent_init.done_ev);
        check(sc_reent_init.bw_calls == 1 && sc_reent_init.saw_ok &&
              sc_reent_init.end_resp_fw_sync == tlm::TLM_COMPLETED &&
              uvm_reent_tgt->end_resp_seen &&
              uvm_reent_tgt->bw_ret_sync == UVM_TLM_ACCEPTED &&
              sc_reent_init.rx_data == std::vector<unsigned char>({0xD1, 0xD2, 0xD3, 0xD4}) &&
              rdata3[0] == 0xD1 && rdata3[3] == 0xD4,
              "7f.3 target_bridge nb_transport_bw -> sync nb_transport_fw(END_RESP) -> UVM_TLM_COMPLETED");

        // 7f.4 initiator_bridge::nb_transport_bw -> sync nb_transport_fw(END_RESP) -> TLM_COMPLETED
        uvm_reent_init->call_end_resp_in_bw = true;
        uvm_reent_init->bw_calls = 0;
        uvm_reent_init->saw_ok = false;
        uvm_reent_init->rx_data.clear();
        uvm_reent_init->end_resp_fw_sync = UVM_TLM_ACCEPTED;
        sc_reent_tgt.sync_bw_in_fw = false;
        sc_reent_tgt.end_resp_seen = false;

        uvm_tlm_gp uvm_gp4("uvm_gp4");
        uvm_gp4.set_read();
        uvm_gp4.set_address(0x5C);
        uvm_gp4.set_data_length(4);
        uvm_tlm_phase_e ph4 = BEGIN_REQ;
        uvm_tlm_time d4("d4");
        uvm_tlm_sync_e sync4 = uvm_reent_init->nb_skt.nb_transport_fw(uvm_gp4, ph4, d4);
        check(sync4 == UVM_TLM_ACCEPTED, "7f.4 initiator_bridge initial BEGIN_REQ accepted");
        sc_core::wait(uvm_reent_init->done_ev);
        check(uvm_reent_init->bw_calls == 1 && uvm_reent_init->saw_ok &&
              uvm_reent_init->end_resp_fw_sync == UVM_TLM_COMPLETED &&
              sc_reent_tgt.end_resp_seen &&
              sc_reent_tgt.bw_ret_sync == tlm::TLM_ACCEPTED &&
              uvm_reent_init->rx_data == std::vector<unsigned char>({0xF1, 0xF2, 0xF3, 0xF4}) &&
              uvm_gp4.m_data == std::vector<unsigned char>({0xF1, 0xF2, 0xF3, 0xF4}),
              "7f.4 initiator_bridge nb_transport_bw -> sync nb_transport_fw(END_RESP) -> TLM_COMPLETED");
      }
    }

    uvm_report_cb::do_delete(nullptr, &catcher);

    std::cout << "tlm2_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
    phase.drop_objection(this);
  }

  virtual void report_phase(uvm_phase& phase)
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
  run_test("tlm2_test");
  return 0;
}
