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

#ifndef UVM_TLM_B_SOCKETS_H_
#define UVM_TLM_B_SOCKETS_H_

#include <string>
#include <type_traits>
#include <systemc>

#include "uvmsc/base/uvm_component.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/tlm2/uvm_tlm2_ifs.h"

namespace uvm {

template <typename T> class uvm_tlm_b_target_socket_base;
template <typename T> class uvm_tlm_b_passthrough_initiator_socket;
template <typename T> class uvm_tlm_b_passthrough_target_socket;

//----------------------------------------------------------------------
// Class: uvm_tlm_b_target_socket_base<T>
//
//! Non-IMP-parameterized base for blocking target sockets.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload>
class uvm_tlm_b_target_socket_base
  : public sc_core::sc_export<uvm_tlm_if_base<T, uvm_tlm_phase_e> >,
    public virtual uvm_tlm_if_base<T, uvm_tlm_phase_e>
{
 public:
  typedef uvm_tlm_if_base<T, uvm_tlm_phase_e> if_type;

  explicit uvm_tlm_b_target_socket_base( const std::string& name )
    : sc_core::sc_export<if_type>(name.c_str())
  {
    sc_core::sc_export<if_type>::bind(*this);
  }

  virtual ~uvm_tlm_b_target_socket_base() {}

  virtual const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_b_target_socket_base"; }
  virtual const char* kind() const { return "uvm_tlm_b_target_socket"; }
};

//----------------------------------------------------------------------
// Class: uvm_tlm_b_initiator_socket<T>
//
//! Blocking TLM-2 initiator socket per IEEE 1800.2-2020 section 12.4.2.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload>
class uvm_tlm_b_initiator_socket
  : public sc_core::sc_port<uvm_tlm_if_base<T, uvm_tlm_phase_e>, 1, sc_core::SC_ZERO_OR_MORE_BOUND>
{
 public:
  typedef uvm_tlm_if_base<T, uvm_tlm_phase_e> if_type;
  typedef sc_core::sc_port<if_type, 1, sc_core::SC_ZERO_OR_MORE_BOUND> port_type;

  uvm_tlm_b_initiator_socket()
    : port_type(sc_core::sc_gen_unique_name("uvm_tlm_b_initiator_socket")),
      m_name(this->basename())
  {}

  explicit uvm_tlm_b_initiator_socket( const std::string& name )
    : port_type(name.c_str()),
      m_name(name)
  {}

  virtual ~uvm_tlm_b_initiator_socket() {}

  void connect( if_type& ifs )
  {
    this->bind(ifs);
  }

  void connect( uvm_tlm_b_target_socket_base<T>& tgt )
  {
    this->bind(static_cast<sc_core::sc_export<if_type>&>(tgt));
  }

  void connect( uvm_tlm_b_passthrough_initiator_socket<T>& pt );

  void connect( uvm_tlm_b_passthrough_target_socket<T>& pt );

  void b_transport( T& t, uvm_tlm_time& delay )
  {
    if (this->size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "b_transport called on unbound uvm_tlm_b_initiator_socket '" +
                get_full_name() + "'.");
      return;
    }
    (*this)->b_transport(t, delay);
  }

  const std::string get_name() const { return m_name; }
  const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_b_initiator_socket"; }
  virtual const char* kind() const { return "uvm_tlm_b_initiator_socket"; }

 private:
  std::string m_name;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_b_target_socket<T, IMP>
//
//! Blocking TLM-2 target socket per IEEE 1800.2-2020 section 12.4.2.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload,
          typename IMP = uvm_tlm_if_base<T, uvm_tlm_phase_e> >
class uvm_tlm_b_target_socket : public uvm_tlm_b_target_socket_base<T>
{
 public:
  typedef uvm_tlm_if_base<T, uvm_tlm_phase_e> if_type;

  explicit uvm_tlm_b_target_socket( const std::string& name = "uvm_tlm_b_target_socket",
                                    IMP* imp = nullptr )
    : uvm_tlm_b_target_socket_base<T>(name),
      m_name(name),
      m_imp(imp),
      m_ext_if(nullptr)
  {}

  virtual ~uvm_tlm_b_target_socket() {}

  void set_imp( IMP* imp )
  {
    m_imp = imp;
  }

  void connect( if_type& ifs )
  {
    m_ext_if = &ifs;
  }

  template <typename EXPORT,
            typename std::enable_if<
              std::is_base_of<sc_core::sc_export<if_type>, EXPORT>::value &&
              !std::is_base_of<if_type, EXPORT>::value,
              int>::type = 0>
  void connect( EXPORT& exp )
  {
    m_ext_if = dynamic_cast<if_type*>(exp.get_interface());
  }

  virtual void b_transport( T& t, uvm_tlm_time& delay )
  {
    if (m_imp != nullptr)
    {
      m_imp->b_transport(t, delay);
      return;
    }
    if (m_ext_if != nullptr)
    {
      m_ext_if->b_transport(t, delay);
      return;
    }
    UVM_FATAL("UVM/TLM2/UNBOUND",
              "b_transport called on uvm_tlm_b_target_socket '" +
              this->get_full_name() + "' with no bound implementation.");
  }

  const std::string get_name() const { return m_name; }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_b_target_socket"; }

 private:
  std::string m_name;
  IMP* m_imp;
  if_type* m_ext_if;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_b_passthrough_initiator_socket<T>
//
//! Blocking passthrough initiator socket forwarding across hierarchy.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload>
class uvm_tlm_b_passthrough_initiator_socket
  : public sc_core::sc_export<uvm_tlm_if_base<T, uvm_tlm_phase_e> >,
    public virtual uvm_tlm_if_base<T, uvm_tlm_phase_e>
{
 public:
  typedef uvm_tlm_if_base<T, uvm_tlm_phase_e> if_type;
  typedef sc_core::sc_port<if_type, 1, sc_core::SC_ZERO_OR_MORE_BOUND> port_type;

  explicit uvm_tlm_b_passthrough_initiator_socket(
    const std::string& name = "uvm_tlm_b_passthrough_initiator_socket" )
    : sc_core::sc_export<if_type>(name.c_str()),
      m_port(sc_core::sc_gen_unique_name((name + "_port").c_str())),
      m_name(name)
  {
    sc_core::sc_export<if_type>::bind(*this);
  }

  virtual ~uvm_tlm_b_passthrough_initiator_socket() {}

  void connect( if_type& ifs )
  {
    m_port.bind(ifs);
  }

  void connect( uvm_tlm_b_target_socket_base<T>& tgt )
  {
    m_port.bind(static_cast<sc_core::sc_export<if_type>&>(tgt));
  }

  void connect( uvm_tlm_b_passthrough_initiator_socket<T>& pt )
  {
    m_port.bind(static_cast<sc_core::sc_export<if_type>&>(pt));
  }

  void connect( uvm_tlm_b_passthrough_target_socket<T>& pt );

  virtual void b_transport( T& t, uvm_tlm_time& delay )
  {
    if (m_port.size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "b_transport called on unbound uvm_tlm_b_passthrough_initiator_socket '" +
                get_full_name() + "'.");
      return;
    }
    m_port->b_transport(t, delay);
  }

  const std::string get_name() const { return m_name; }
  const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_b_passthrough_initiator_socket"; }
  virtual const char* kind() const { return "uvm_tlm_b_passthrough_initiator_socket"; }

 private:
  port_type m_port;
  std::string m_name;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_b_passthrough_target_socket<T>
//
//! Blocking passthrough target socket forwarding across hierarchy.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload>
class uvm_tlm_b_passthrough_target_socket
  : public sc_core::sc_export<uvm_tlm_if_base<T, uvm_tlm_phase_e> >,
    public virtual uvm_tlm_if_base<T, uvm_tlm_phase_e>
{
 public:
  typedef uvm_tlm_if_base<T, uvm_tlm_phase_e> if_type;
  typedef sc_core::sc_port<if_type, 1, sc_core::SC_ZERO_OR_MORE_BOUND> port_type;

  explicit uvm_tlm_b_passthrough_target_socket(
    const std::string& name = "uvm_tlm_b_passthrough_target_socket" )
    : sc_core::sc_export<if_type>(name.c_str()),
      m_port(sc_core::sc_gen_unique_name((name + "_port").c_str())),
      m_name(name)
  {
    sc_core::sc_export<if_type>::bind(*this);
  }

  virtual ~uvm_tlm_b_passthrough_target_socket() {}

  void connect( if_type& ifs )
  {
    m_port.bind(ifs);
  }

  void connect( uvm_tlm_b_target_socket_base<T>& tgt )
  {
    m_port.bind(static_cast<sc_core::sc_export<if_type>&>(tgt));
  }

  void connect( uvm_tlm_b_passthrough_target_socket<T>& pt )
  {
    m_port.bind(static_cast<sc_core::sc_export<if_type>&>(pt));
  }

  virtual void b_transport( T& t, uvm_tlm_time& delay )
  {
    if (m_port.size() == 0)
    {
      UVM_FATAL("UVM/TLM2/UNBOUND",
                "b_transport called on unbound uvm_tlm_b_passthrough_target_socket '" +
                get_full_name() + "'.");
      return;
    }
    m_port->b_transport(t, delay);
  }

  const std::string get_name() const { return m_name; }
  const std::string get_full_name() const { return this->name(); }
  virtual const std::string get_type_name() const { return "uvm::uvm_tlm_b_passthrough_target_socket"; }
  virtual const char* kind() const { return "uvm_tlm_b_passthrough_target_socket"; }

 private:
  port_type m_port;
  std::string m_name;
};

template <typename T>
inline void uvm_tlm_b_initiator_socket<T>::connect(
  uvm_tlm_b_passthrough_initiator_socket<T>& pt )
{
  this->bind(static_cast<sc_core::sc_export<if_type>&>(pt));
}

template <typename T>
inline void uvm_tlm_b_initiator_socket<T>::connect(
  uvm_tlm_b_passthrough_target_socket<T>& pt )
{
  this->bind(static_cast<sc_core::sc_export<if_type>&>(pt));
}

template <typename T>
inline void uvm_tlm_b_passthrough_initiator_socket<T>::connect(
  uvm_tlm_b_passthrough_target_socket<T>& pt )
{
  m_port.bind(static_cast<sc_core::sc_export<if_type>&>(pt));
}

} // namespace uvm

#endif // UVM_TLM_B_SOCKETS_H_
