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

#ifndef UVM_TLM_NB_SOCKETS_H_
#define UVM_TLM_NB_SOCKETS_H_

#include <string>
#include <systemc>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/tlm2/uvm_tlm2_ifs.h"

namespace uvm {

template <typename T, typename P> class uvm_tlm_nb_target_socket_base;
template <typename T, typename P> class uvm_tlm_nb_passthrough_initiator_socket;
template <typename T, typename P> class uvm_tlm_nb_passthrough_target_socket;

//----------------------------------------------------------------------
// Class: uvm_tlm_nb_initiator_socket_base<T, P>
//
//! Non-IMP-parameterized base for non-blocking initiator sockets.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload, typename P = uvm_tlm_phase_e>
class uvm_tlm_nb_initiator_socket_base
  : public sc_core::sc_port<uvm_tlm_if_base<T, P>, 1, sc_core::SC_ZERO_OR_MORE_BOUND>,
    public virtual uvm_tlm_if_base<T, P>
{
 public:
  typedef uvm_tlm_if_base<T, P> if_type;
  typedef sc_core::sc_port<if_type, 1, sc_core::SC_ZERO_OR_MORE_BOUND> port_type;
  typedef sc_core::sc_export<if_type> export_type;

  explicit uvm_tlm_nb_initiator_socket_base( const std::string& name )
    : port_type(name.c_str()),
      m_bw_export(sc_core::sc_gen_unique_name((name + "_bw_export").c_str())),
      m_name(name)
  {
    m_bw_export.bind(*this);
  }

  virtual ~uvm_tlm_nb_initiator_socket_base() {}

  export_type& get_bw_export() { return m_bw_export; }
  port_type& get_fw_port() { return *this; }

  const std::string get_name() const { return m_name; }
  const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_nb_initiator_socket_base"; }
  virtual const char* kind() const { return "uvm_tlm_nb_initiator_socket"; }

 protected:
  export_type m_bw_export;
  std::string m_name;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_nb_target_socket_base<T, P>
//
//! Non-IMP-parameterized base for non-blocking target sockets.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload, typename P = uvm_tlm_phase_e>
class uvm_tlm_nb_target_socket_base
  : public sc_core::sc_export<uvm_tlm_if_base<T, P> >,
    public virtual uvm_tlm_if_base<T, P>
{
 public:
  typedef uvm_tlm_if_base<T, P> if_type;
  typedef sc_core::sc_port<if_type, 1, sc_core::SC_ZERO_OR_MORE_BOUND> port_type;
  typedef sc_core::sc_export<if_type> export_type;

  explicit uvm_tlm_nb_target_socket_base( const std::string& name )
    : export_type(name.c_str()),
      m_bw_port(sc_core::sc_gen_unique_name((name + "_bw_port").c_str())),
      m_name(name)
  {
    export_type::bind(*this);
  }

  virtual ~uvm_tlm_nb_target_socket_base() {}

  export_type& get_fw_export() { return *this; }
  port_type& get_bw_port() { return m_bw_port; }

  const std::string get_name() const { return m_name; }
  const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_nb_target_socket_base"; }
  virtual const char* kind() const { return "uvm_tlm_nb_target_socket"; }

 protected:
  port_type m_bw_port;
  std::string m_name;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_nb_initiator_socket<T, P, IMP>
//
//! Non-blocking TLM-2 initiator socket per IEEE 1800.2-2020 section 12.4.2.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload,
          typename P = uvm_tlm_phase_e,
          typename IMP = uvm_tlm_if_base<T, P> >
class uvm_tlm_nb_initiator_socket : public uvm_tlm_nb_initiator_socket_base<T, P>
{
 public:
  typedef uvm_tlm_if_base<T, P> if_type;

  explicit uvm_tlm_nb_initiator_socket(
    const std::string& name = "uvm_tlm_nb_initiator_socket",
    IMP* imp = nullptr )
    : uvm_tlm_nb_initiator_socket_base<T, P>(name),
      m_imp(imp),
      m_ext_bw_if(nullptr)
  {}

  virtual ~uvm_tlm_nb_initiator_socket() {}

  void set_imp( IMP* imp ) { m_imp = imp; }

  void connect( if_type& bw_if )
  {
    m_ext_bw_if = &bw_if;
  }

  void connect( uvm_tlm_nb_target_socket_base<T, P>& tgt )
  {
    this->bind(tgt.get_fw_export());
    tgt.get_bw_port().bind(this->get_bw_export());
  }

  void connect( uvm_tlm_nb_passthrough_initiator_socket<T, P>& pt );
  void connect( uvm_tlm_nb_passthrough_target_socket<T, P>& pt );

  virtual uvm_tlm_sync_e nb_transport_fw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (this->size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "nb_transport_fw called on unbound uvm_tlm_nb_initiator_socket '" +
                this->get_full_name() + "'.");
      return UVM_TLM_COMPLETED;
    }
    return (*this)->nb_transport_fw(t, p, delay);
  }

  virtual uvm_tlm_sync_e nb_transport_bw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_imp != nullptr)
      return m_imp->nb_transport_bw(t, p, delay);
    if (m_ext_bw_if != nullptr)
      return m_ext_bw_if->nb_transport_bw(t, p, delay);
    UVM_FATAL("UVM/TLM2/UNBOUND",
              "nb_transport_bw called on uvm_tlm_nb_initiator_socket '" +
              this->get_full_name() + "' with no bound backward implementation.");
    return UVM_TLM_COMPLETED;
  }

  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_nb_initiator_socket"; }

 private:
  IMP* m_imp;
  if_type* m_ext_bw_if;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_nb_target_socket<T, P, IMP>
//
//! Non-blocking TLM-2 target socket per IEEE 1800.2-2020 section 12.4.2.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload,
          typename P = uvm_tlm_phase_e,
          typename IMP = uvm_tlm_if_base<T, P> >
class uvm_tlm_nb_target_socket : public uvm_tlm_nb_target_socket_base<T, P>
{
 public:
  typedef uvm_tlm_if_base<T, P> if_type;

  explicit uvm_tlm_nb_target_socket(
    const std::string& name = "uvm_tlm_nb_target_socket",
    IMP* imp = nullptr )
    : uvm_tlm_nb_target_socket_base<T, P>(name),
      m_imp(imp),
      m_ext_fw_if(nullptr)
  {}

  virtual ~uvm_tlm_nb_target_socket() {}

  void set_imp( IMP* imp ) { m_imp = imp; }

  void connect( if_type& fw_if )
  {
    m_ext_fw_if = &fw_if;
  }

  void connect( uvm_tlm_nb_initiator_socket_base<T, P>& init )
  {
    init.get_fw_port().bind(this->get_fw_export());
    this->get_bw_port().bind(init.get_bw_export());
  }

  virtual uvm_tlm_sync_e nb_transport_fw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_imp != nullptr)
      return m_imp->nb_transport_fw(t, p, delay);
    if (m_ext_fw_if != nullptr)
      return m_ext_fw_if->nb_transport_fw(t, p, delay);
    UVM_FATAL("UVM/TLM2/UNBOUND",
              "nb_transport_fw called on uvm_tlm_nb_target_socket '" +
              this->get_full_name() + "' with no bound forward implementation.");
    return UVM_TLM_COMPLETED;
  }

  virtual uvm_tlm_sync_e nb_transport_bw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (this->m_bw_port.size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "nb_transport_bw called on unbound uvm_tlm_nb_target_socket '" +
                this->get_full_name() + "'.");
      return UVM_TLM_COMPLETED;
    }
    return this->m_bw_port->nb_transport_bw(t, p, delay);
  }

  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_nb_target_socket"; }

 private:
  IMP* m_imp;
  if_type* m_ext_fw_if;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_nb_passthrough_initiator_socket<T, P>
//
//! Non-blocking passthrough initiator socket forwarding across hierarchy.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload, typename P = uvm_tlm_phase_e>
class uvm_tlm_nb_passthrough_initiator_socket
  : public sc_core::sc_export<uvm_tlm_if_base<T, P> >,
    public virtual uvm_tlm_if_base<T, P>
{
 public:
  typedef uvm_tlm_if_base<T, P> if_type;
  typedef sc_core::sc_port<if_type, 1, sc_core::SC_ZERO_OR_MORE_BOUND> port_type;
  typedef sc_core::sc_export<if_type> export_type;

  explicit uvm_tlm_nb_passthrough_initiator_socket(
    const std::string& name = "uvm_tlm_nb_passthrough_initiator_socket" )
    : export_type(name.c_str()),
      m_fw_port(sc_core::sc_gen_unique_name((name + "_fw_port").c_str())),
      m_bw_port(sc_core::sc_gen_unique_name((name + "_bw_port").c_str())),
      m_bw_export(sc_core::sc_gen_unique_name((name + "_bw_export").c_str())),
      m_name(name)
  {
    export_type::bind(*this);
    m_bw_export.bind(*this);
  }

  virtual ~uvm_tlm_nb_passthrough_initiator_socket() {}

  export_type& get_fw_export() { return *this; }
  export_type& get_bw_export() { return m_bw_export; }
  port_type& get_bw_port() { return m_bw_port; }

  void connect( uvm_tlm_nb_target_socket_base<T, P>& tgt )
  {
    m_fw_port.bind(tgt.get_fw_export());
    tgt.get_bw_port().bind(m_bw_export);
  }

  void connect( uvm_tlm_nb_passthrough_initiator_socket<T, P>& pt )
  {
    m_fw_port.bind(pt.get_fw_export());
    pt.get_bw_port().bind(m_bw_export);
  }

  void connect( uvm_tlm_nb_passthrough_target_socket<T, P>& pt );

  virtual uvm_tlm_sync_e nb_transport_fw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_fw_port.size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "nb_transport_fw called on unbound uvm_tlm_nb_passthrough_initiator_socket '" +
                get_full_name() + "'.");
      return UVM_TLM_COMPLETED;
    }
    return m_fw_port->nb_transport_fw(t, p, delay);
  }

  virtual uvm_tlm_sync_e nb_transport_bw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_bw_port.size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "nb_transport_bw called on unbound uvm_tlm_nb_passthrough_initiator_socket '" +
                get_full_name() + "'.");
      return UVM_TLM_COMPLETED;
    }
    return m_bw_port->nb_transport_bw(t, p, delay);
  }

  const std::string get_name() const { return m_name; }
  const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_nb_passthrough_initiator_socket"; }
  virtual const char* kind() const { return "uvm_tlm_nb_passthrough_initiator_socket"; }

 private:
  port_type m_fw_port;
  port_type m_bw_port;
  export_type m_bw_export;
  std::string m_name;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_nb_passthrough_target_socket<T, P>
//
//! Non-blocking passthrough target socket forwarding across hierarchy.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload, typename P = uvm_tlm_phase_e>
class uvm_tlm_nb_passthrough_target_socket
  : public sc_core::sc_export<uvm_tlm_if_base<T, P> >,
    public virtual uvm_tlm_if_base<T, P>
{
 public:
  typedef uvm_tlm_if_base<T, P> if_type;
  typedef sc_core::sc_port<if_type, 1, sc_core::SC_ZERO_OR_MORE_BOUND> port_type;
  typedef sc_core::sc_export<if_type> export_type;

  explicit uvm_tlm_nb_passthrough_target_socket(
    const std::string& name = "uvm_tlm_nb_passthrough_target_socket" )
    : export_type(name.c_str()),
      m_fw_port(sc_core::sc_gen_unique_name((name + "_fw_port").c_str())),
      m_bw_port(sc_core::sc_gen_unique_name((name + "_bw_port").c_str())),
      m_bw_export(sc_core::sc_gen_unique_name((name + "_bw_export").c_str())),
      m_name(name)
  {
    export_type::bind(*this);
    m_bw_export.bind(*this);
  }

  virtual ~uvm_tlm_nb_passthrough_target_socket() {}

  export_type& get_fw_export() { return *this; }
  export_type& get_bw_export() { return m_bw_export; }
  port_type& get_bw_port() { return m_bw_port; }

  void connect( uvm_tlm_nb_target_socket_base<T, P>& tgt )
  {
    m_fw_port.bind(tgt.get_fw_export());
    tgt.get_bw_port().bind(m_bw_export);
  }

  void connect( uvm_tlm_nb_passthrough_target_socket<T, P>& pt )
  {
    m_fw_port.bind(pt.get_fw_export());
    pt.get_bw_port().bind(m_bw_export);
  }

  virtual uvm_tlm_sync_e nb_transport_fw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_fw_port.size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "nb_transport_fw called on unbound uvm_tlm_nb_passthrough_target_socket '" +
                get_full_name() + "'.");
      return UVM_TLM_COMPLETED;
    }
    return m_fw_port->nb_transport_fw(t, p, delay);
  }

  virtual uvm_tlm_sync_e nb_transport_bw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_bw_port.size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "nb_transport_bw called on unbound uvm_tlm_nb_passthrough_target_socket '" +
                get_full_name() + "'.");
      return UVM_TLM_COMPLETED;
    }
    return m_bw_port->nb_transport_bw(t, p, delay);
  }

  const std::string get_name() const { return m_name; }
  const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_nb_passthrough_target_socket"; }
  virtual const char* kind() const { return "uvm_tlm_nb_passthrough_target_socket"; }

 private:
  port_type m_fw_port;
  port_type m_bw_port;
  export_type m_bw_export;
  std::string m_name;
};

template <typename T, typename P, typename IMP>
inline void uvm_tlm_nb_initiator_socket<T, P, IMP>::connect(
  uvm_tlm_nb_passthrough_initiator_socket<T, P>& pt )
{
  this->bind(pt.get_fw_export());
  pt.get_bw_port().bind(this->get_bw_export());
}

template <typename T, typename P, typename IMP>
inline void uvm_tlm_nb_initiator_socket<T, P, IMP>::connect(
  uvm_tlm_nb_passthrough_target_socket<T, P>& pt )
{
  this->bind(pt.get_fw_export());
  pt.get_bw_port().bind(this->get_bw_export());
}

template <typename T, typename P>
inline void uvm_tlm_nb_passthrough_initiator_socket<T, P>::connect(
  uvm_tlm_nb_passthrough_target_socket<T, P>& pt )
{
  m_fw_port.bind(pt.get_fw_export());
  pt.get_bw_port().bind(m_bw_export);
}

} // namespace uvm

#endif // UVM_TLM_NB_SOCKETS_H_
