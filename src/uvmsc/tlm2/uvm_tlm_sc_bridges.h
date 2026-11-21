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

#ifndef UVM_TLM_SC_BRIDGES_H_
#define UVM_TLM_SC_BRIDGES_H_

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/tlm2/uvm_tlm2_defines.h"
#include "uvmsc/tlm2/uvm_tlm_gp.h"
#include "uvmsc/tlm2/uvm_tlm_time.h"
#include "uvmsc/tlm2/uvm_tlm_b_sockets.h"
#include "uvmsc/tlm2/uvm_tlm_nb_sockets.h"

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_tlm_sc_initiator_socket<BUSWIDTH, TYPES, N, POL>
//
//! UVM-friendly wrapper around tlm::tlm_initiator_socket supporting
//! std::string construction and UVM connect() syntax. Does not override
//! final tlm_base_initiator_socket::is_multi_socket().
//----------------------------------------------------------------------

template <unsigned int BUSWIDTH = 32,
          typename TYPES = tlm::tlm_base_protocol_types,
          int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class uvm_tlm_sc_initiator_socket
  : public tlm::tlm_initiator_socket<BUSWIDTH, TYPES, N, POL>
{
  typedef tlm::tlm_initiator_socket<BUSWIDTH, TYPES, N, POL> base_type;

 public:
  uvm_tlm_sc_initiator_socket()
    : base_type(), m_name(this->basename()) {}

  explicit uvm_tlm_sc_initiator_socket( const std::string& name )
    : base_type(name.c_str()), m_name(name) {}

  virtual ~uvm_tlm_sc_initiator_socket() {}

  template <typename TARGET_OR_IF>
  void connect( TARGET_OR_IF& target )
  {
    this->bind(target);
  }

  const std::string get_name() const { return m_name; }
  const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_sc_initiator_socket"; }
  virtual const char* kind() const { return "uvm_tlm_sc_initiator_socket"; }

 private:
  std::string m_name;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_sc_target_socket<BUSWIDTH, TYPES, N, POL>
//
//! UVM-friendly wrapper around tlm::tlm_target_socket supporting
//! std::string construction and UVM connect() syntax. Does not override
//! final tlm_base_target_socket::is_multi_socket().
//----------------------------------------------------------------------

template <unsigned int BUSWIDTH = 32,
          typename TYPES = tlm::tlm_base_protocol_types,
          int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class uvm_tlm_sc_target_socket
  : public tlm::tlm_target_socket<BUSWIDTH, TYPES, N, POL>
{
  typedef tlm::tlm_target_socket<BUSWIDTH, TYPES, N, POL> base_type;

 public:
  uvm_tlm_sc_target_socket()
    : base_type(), m_name(this->basename()) {}

  explicit uvm_tlm_sc_target_socket( const std::string& name )
    : base_type(name.c_str()), m_name(name) {}

  virtual ~uvm_tlm_sc_target_socket() {}

  template <typename INIT_OR_IF>
  void connect( INIT_OR_IF& init )
  {
    this->bind(init);
  }

  const std::string get_name() const { return m_name; }
  const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_sc_target_socket"; }
  virtual const char* kind() const { return "uvm_tlm_sc_target_socket"; }

 private:
  std::string m_name;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_sc_initiator_bridge<BUSWIDTH>
//
//! Translates UVM TLM-2 sockets (uvm_tlm_gp / uvm_tlm_time) to a
//! SystemC 3.0.3 tlm::tlm_initiator_socket<BUSWIDTH>.
//----------------------------------------------------------------------

template <unsigned int BUSWIDTH = 32>
class uvm_tlm_sc_initiator_bridge
  : public sc_core::sc_module,
    public virtual tlm::tlm_bw_transport_if<>
{
 public:
  uvm_tlm_b_target_socket<uvm_tlm_gp, uvm_tlm_sc_initiator_bridge<BUSWIDTH> > uvm_b_target_skt;
  uvm_tlm_nb_target_socket<uvm_tlm_gp, uvm_tlm_phase_e, uvm_tlm_sc_initiator_bridge<BUSWIDTH> > uvm_nb_target_skt;
  tlm::tlm_initiator_socket<BUSWIDTH, tlm::tlm_base_protocol_types, 1, sc_core::SC_ZERO_OR_MORE_BOUND> sc_init_skt;

  explicit uvm_tlm_sc_initiator_bridge( sc_core::sc_module_name name )
    : sc_core::sc_module(name),
      uvm_b_target_skt("uvm_b_target_skt", this),
      uvm_nb_target_skt("uvm_nb_target_skt", this),
      sc_init_skt("sc_init_skt")
  {
    sc_init_skt.bind(*this);
  }

  virtual ~uvm_tlm_sc_initiator_bridge() {}

  void b_transport( uvm_tlm_gp& gp, uvm_tlm_time& delay )
  {
    if (sc_init_skt.size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "uvm_tlm_sc_initiator_bridge::sc_init_skt is unbound.");
      return;
    }
    tlm::tlm_generic_payload sc_gp;
    const std::size_t req_len = static_cast<std::size_t>(gp.get_data_length());
    const std::size_t data_sz = (gp.is_read() || !gp.m_data.empty())
                                  ? std::max(req_len, gp.m_data.size())
                                  : 0;
    std::vector<unsigned char> data_buf(data_sz, 0);
    const std::size_t be_sz = gp.m_byte_enable.empty()
                                ? 0u
                                : std::max(static_cast<std::size_t>(gp.get_byte_enable_length()),
                                           gp.m_byte_enable.size());
    std::vector<unsigned char> be_buf(be_sz, UVM_TLM_BYTE_DISABLED);
    gp.to_tlm_gp(sc_gp,
                 data_buf.empty() ? nullptr : &data_buf[0],
                 be_buf.empty() ? nullptr : &be_buf[0]);
    if (data_buf.empty())
    {
      sc_gp.set_data_ptr(nullptr);
      sc_gp.set_data_length(0);
    }
    if (be_buf.empty())
    {
      sc_gp.set_byte_enable_ptr(nullptr);
      sc_gp.set_byte_enable_length(0);
    }

    sc_core::sc_time sc_delay = delay.to_sc_time();
    sc_init_skt->b_transport(sc_gp, sc_delay);
    delay.from_sc_time(sc_delay);
    gp.from_tlm_gp(sc_gp);
  }

  uvm_tlm_sync_e nb_transport_fw( uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay )
  {
    if (sc_init_skt.size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "uvm_tlm_sc_initiator_bridge::sc_init_skt is unbound.");
      return UVM_TLM_COMPLETED;
    }
    std::shared_ptr<nb_ctx> ctx = m_find_or_create_ctx(&gp);
    const std::size_t req_len = static_cast<std::size_t>(gp.get_data_length());
    const std::size_t data_sz = (gp.is_read() || !gp.m_data.empty())
                                  ? std::max(req_len, gp.m_data.size())
                                  : 0;
    ctx->data_buf.assign(data_sz, 0);
    const std::size_t be_sz = gp.m_byte_enable.empty()
                                ? 0u
                                : std::max(static_cast<std::size_t>(gp.get_byte_enable_length()),
                                           gp.m_byte_enable.size());
    ctx->be_buf.assign(be_sz, UVM_TLM_BYTE_DISABLED);
    gp.to_tlm_gp(ctx->sc_gp,
                 ctx->data_buf.empty() ? nullptr : &ctx->data_buf[0],
                 ctx->be_buf.empty() ? nullptr : &ctx->be_buf[0]);
    if (ctx->data_buf.empty())
    {
      ctx->sc_gp.set_data_ptr(nullptr);
      ctx->sc_gp.set_data_length(0);
    }
    if (ctx->be_buf.empty())
    {
      ctx->sc_gp.set_byte_enable_ptr(nullptr);
      ctx->sc_gp.set_byte_enable_length(0);
    }
    tlm::tlm_phase sc_ph(static_cast<tlm::tlm_phase_enum>(phase));
    sc_core::sc_time sc_delay = delay.to_sc_time();

    tlm::tlm_sync_enum sync = sc_init_skt->nb_transport_fw(ctx->sc_gp, sc_ph, sc_delay);

    if (!ctx->completed)
    {
      gp.from_tlm_gp(ctx->sc_gp);
      delay.from_sc_time(sc_delay);
      phase = static_cast<uvm_tlm_phase_e>(static_cast<unsigned int>(sc_ph));

      if (sync == tlm::TLM_COMPLETED)
      {
        ctx->completed = true;
        m_erase_ctx(ctx.get());
      }
    }

    return static_cast<uvm_tlm_sync_e>(sync);
  }

  virtual tlm::tlm_sync_enum nb_transport_bw( tlm::tlm_generic_payload& sc_gp,
                                              tlm::tlm_phase& sc_ph,
                                              sc_core::sc_time& sc_delay )
  {
    std::shared_ptr<nb_ctx> ctx = m_find_by_sc_gp(&sc_gp);
    if (!ctx || ctx->completed || ctx->uvm_gp == nullptr)
      return tlm::TLM_COMPLETED;

    ctx->uvm_gp->from_tlm_gp(sc_gp);
    uvm_tlm_phase_e uvm_ph = static_cast<uvm_tlm_phase_e>(static_cast<unsigned int>(sc_ph));
    uvm_tlm_time uvm_delay("sc_bw_delay");
    uvm_delay.from_sc_time(sc_delay);

    uvm_tlm_sync_e sync = uvm_nb_target_skt.nb_transport_bw(*ctx->uvm_gp, uvm_ph, uvm_delay);

    if (!ctx->completed)
    {
      const std::size_t bw_len = static_cast<std::size_t>(ctx->uvm_gp->get_data_length());
      const std::size_t bw_sz = (ctx->uvm_gp->is_read() || !ctx->uvm_gp->m_data.empty())
                                  ? std::max(bw_len, ctx->uvm_gp->m_data.size())
                                  : 0;
      ctx->data_buf.assign(bw_sz, 0);
      const std::size_t bw_be_sz =
        ctx->uvm_gp->m_byte_enable.empty()
          ? 0u
          : std::max(static_cast<std::size_t>(ctx->uvm_gp->get_byte_enable_length()),
                     ctx->uvm_gp->m_byte_enable.size());
      ctx->be_buf.assign(bw_be_sz, UVM_TLM_BYTE_DISABLED);
      ctx->uvm_gp->to_tlm_gp(sc_gp,
                             ctx->data_buf.empty() ? nullptr : &ctx->data_buf[0],
                             ctx->be_buf.empty() ? nullptr : &ctx->be_buf[0]);
      if (ctx->data_buf.empty())
      {
        sc_gp.set_data_ptr(nullptr);
        sc_gp.set_data_length(0);
      }
      if (ctx->be_buf.empty())
      {
        sc_gp.set_byte_enable_ptr(nullptr);
        sc_gp.set_byte_enable_length(0);
      }
      sc_ph = static_cast<tlm::tlm_phase_enum>(uvm_ph);
      sc_delay = uvm_delay.to_sc_time();

      if (sync == UVM_TLM_COMPLETED)
      {
        ctx->completed = true;
        m_erase_ctx(ctx.get());
      }
    }

    return static_cast<tlm::tlm_sync_enum>(sync);
  }

  virtual void invalidate_direct_mem_ptr( sc_dt::uint64 start_range, sc_dt::uint64 end_range )
  {
    (void)start_range;
    (void)end_range;
  }

 private:
  struct nb_ctx
  {
    uvm_tlm_gp* uvm_gp = nullptr;
    tlm::tlm_generic_payload sc_gp;
    std::vector<unsigned char> data_buf;
    std::vector<unsigned char> be_buf;
    bool completed = false;
  };

  std::shared_ptr<nb_ctx> m_find_or_create_ctx( uvm_tlm_gp* uvm_gp )
  {
    for (auto it = m_nb_ctxs.begin(); it != m_nb_ctxs.end(); ++it)
    {
      if ((*it)->uvm_gp == uvm_gp && !(*it)->completed)
        return *it;
    }
    std::shared_ptr<nb_ctx> ctx = std::make_shared<nb_ctx>();
    ctx->uvm_gp = uvm_gp;
    m_nb_ctxs.push_back(ctx);
    return ctx;
  }

  std::shared_ptr<nb_ctx> m_find_by_sc_gp( const tlm::tlm_generic_payload* sc_gp )
  {
    for (auto it = m_nb_ctxs.begin(); it != m_nb_ctxs.end(); ++it)
    {
      if (&((*it)->sc_gp) == sc_gp && !(*it)->completed)
        return *it;
    }
    return std::shared_ptr<nb_ctx>();
  }

  void m_erase_ctx( const nb_ctx* ctx )
  {
    for (auto it = m_nb_ctxs.begin(); it != m_nb_ctxs.end(); ++it)
    {
      if (it->get() == ctx)
      {
        m_nb_ctxs.erase(it);
        return;
      }
    }
  }

  std::vector<std::shared_ptr<nb_ctx> > m_nb_ctxs;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_sc_target_bridge<BUSWIDTH>
//
//! Translates SystemC 3.0.3 tlm::tlm_target_socket<BUSWIDTH> calls
//! into UVM TLM-2 initiator socket calls (uvm_tlm_gp / uvm_tlm_time).
//----------------------------------------------------------------------

template <unsigned int BUSWIDTH = 32>
class uvm_tlm_sc_target_bridge
  : public sc_core::sc_module,
    public virtual tlm::tlm_fw_transport_if<>
{
 public:
  tlm::tlm_target_socket<BUSWIDTH, tlm::tlm_base_protocol_types, 1, sc_core::SC_ZERO_OR_MORE_BOUND> sc_target_skt;
  uvm_tlm_b_initiator_socket<uvm_tlm_gp> uvm_b_init_skt;
  uvm_tlm_nb_initiator_socket<uvm_tlm_gp, uvm_tlm_phase_e, uvm_tlm_sc_target_bridge<BUSWIDTH> > uvm_nb_init_skt;

  explicit uvm_tlm_sc_target_bridge( sc_core::sc_module_name name )
    : sc_core::sc_module(name),
      sc_target_skt("sc_target_skt"),
      uvm_b_init_skt("uvm_b_init_skt"),
      uvm_nb_init_skt("uvm_nb_init_skt", this)
  {
    sc_target_skt.bind(*this);
  }

  virtual ~uvm_tlm_sc_target_bridge() {}

  virtual void b_transport( tlm::tlm_generic_payload& sc_gp, sc_core::sc_time& sc_delay )
  {
    uvm_tlm_gp uvm_gp("bridge_gp");
    uvm_gp.from_tlm_gp(sc_gp);
    uvm_tlm_time uvm_delay("bridge_delay");
    uvm_delay.from_sc_time(sc_delay);

    uvm_b_init_skt.b_transport(uvm_gp, uvm_delay);

    sc_delay = uvm_delay.to_sc_time();
    uvm_gp.to_tlm_gp_in_place(sc_gp);
  }

  virtual tlm::tlm_sync_enum nb_transport_fw( tlm::tlm_generic_payload& sc_gp,
                                              tlm::tlm_phase& sc_ph,
                                              sc_core::sc_time& sc_delay )
  {
    std::shared_ptr<nb_ctx> ctx = m_find_or_create_ctx(&sc_gp);
    ctx->uvm_gp.from_tlm_gp(sc_gp);
    uvm_tlm_phase_e uvm_ph = static_cast<uvm_tlm_phase_e>(static_cast<unsigned int>(sc_ph));
    uvm_tlm_time uvm_delay("sc_fw_delay");
    uvm_delay.from_sc_time(sc_delay);

    uvm_tlm_sync_e sync = uvm_nb_init_skt.nb_transport_fw(ctx->uvm_gp, uvm_ph, uvm_delay);

    if (!ctx->completed)
    {
      ctx->uvm_gp.to_tlm_gp_in_place(sc_gp);
      sc_ph = static_cast<tlm::tlm_phase_enum>(uvm_ph);
      sc_delay = uvm_delay.to_sc_time();

      if (sync == UVM_TLM_COMPLETED)
      {
        ctx->completed = true;
        m_erase_ctx(ctx.get());
      }
    }

    return static_cast<tlm::tlm_sync_enum>(sync);
  }

  uvm_tlm_sync_e nb_transport_bw( uvm_tlm_gp& gp, uvm_tlm_phase_e& phase, uvm_tlm_time& delay )
  {
    std::shared_ptr<nb_ctx> ctx = m_find_by_uvm_gp(&gp);
    if (!ctx || ctx->completed || ctx->sc_gp == nullptr)
      return UVM_TLM_COMPLETED;

    gp.to_tlm_gp_in_place(*ctx->sc_gp);
    tlm::tlm_phase sc_ph(static_cast<tlm::tlm_phase_enum>(phase));
    sc_core::sc_time sc_delay = delay.to_sc_time();

    tlm::tlm_sync_enum sync = sc_target_skt->nb_transport_bw(*ctx->sc_gp, sc_ph, sc_delay);

    if (!ctx->completed)
    {
      gp.from_tlm_gp(*ctx->sc_gp);
      phase = static_cast<uvm_tlm_phase_e>(static_cast<unsigned int>(sc_ph));
      delay.from_sc_time(sc_delay);

      if (sync == tlm::TLM_COMPLETED)
      {
        ctx->completed = true;
        m_erase_ctx(ctx.get());
      }
    }

    return static_cast<uvm_tlm_sync_e>(sync);
  }

  virtual bool get_direct_mem_ptr( tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data )
  {
    (void)trans;
    (void)dmi_data;
    return false;
  }

  virtual unsigned int transport_dbg( tlm::tlm_generic_payload& trans )
  {
    (void)trans;
    return 0;
  }

 private:
  struct nb_ctx
  {
    tlm::tlm_generic_payload* sc_gp = nullptr;
    uvm_tlm_gp uvm_gp;
    bool completed = false;
    nb_ctx() : uvm_gp("bridge_nb_gp") {}
  };

  std::shared_ptr<nb_ctx> m_find_or_create_ctx( tlm::tlm_generic_payload* sc_gp )
  {
    for (auto it = m_nb_ctxs.begin(); it != m_nb_ctxs.end(); ++it)
    {
      if ((*it)->sc_gp == sc_gp && !(*it)->completed)
        return *it;
    }
    std::shared_ptr<nb_ctx> ctx = std::make_shared<nb_ctx>();
    ctx->sc_gp = sc_gp;
    m_nb_ctxs.push_back(ctx);
    return ctx;
  }

  std::shared_ptr<nb_ctx> m_find_by_uvm_gp( const uvm_tlm_gp* uvm_gp )
  {
    for (auto it = m_nb_ctxs.begin(); it != m_nb_ctxs.end(); ++it)
    {
      if (&((*it)->uvm_gp) == uvm_gp && !(*it)->completed)
        return *it;
    }
    return std::shared_ptr<nb_ctx>();
  }

  void m_erase_ctx( const nb_ctx* ctx )
  {
    for (auto it = m_nb_ctxs.begin(); it != m_nb_ctxs.end(); ++it)
    {
      if (it->get() == ctx)
      {
        m_nb_ctxs.erase(it);
        return;
      }
    }
  }

  std::vector<std::shared_ptr<nb_ctx> > m_nb_ctxs;
};

} // namespace uvm

#endif // UVM_TLM_SC_BRIDGES_H_
