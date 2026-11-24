//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2015-2024 NVIDIA Corporation
//   Copyright 2010-2018 Synopsys, Inc.
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

#ifndef UVM_BIDIR_PORTS_H_
#define UVM_BIDIR_PORTS_H_

#include <systemc>
#include <tlm>

#include <string>

#include "uvmsc/base/uvm_port_base.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/tlm1/uvm_ports.h"
#include "uvmsc/tlm1/uvm_tlm_transport_ifs.h"

namespace uvm {

//---------------------------------------------------------------------------
// macros for the bidirectional port implementations
// The UVM put/get/peek/transport API has been implemented, as well as the
// SystemC equivalent functions for compatibility reasons. The put member
// functions convey type PUT_T, the get and peek member functions GET_T.
//---------------------------------------------------------------------------

#define UVM_BLOCKING_TRANSPORT_PORT_METHODS \
  void transport( const REQ& req, RSP& rsp ) { get_imp()->transport(req, rsp); } \
  RSP transport( const REQ& req ) { return get_imp()->transport(req); }

#define UVM_NONBLOCKING_TRANSPORT_PORT_METHODS \
  bool nb_transport( const REQ& req, RSP& rsp ) { return get_imp()->nb_transport(req, rsp); }

#define UVM_BIDIR_BLOCKING_PUT_PORT_METHODS(PUT_T) \
  void put( const PUT_T& val ) { get_imp()->put(val); }

#define UVM_BIDIR_BLOCKING_GET_PEEK_PORT_METHODS(GET_T) \
  void get( GET_T& val )  { val = get_imp()->get(); } \
  GET_T get( tlm::tlm_tag<GET_T> *t = 0 ) { return get_imp()->get(); } \
  void peek( GET_T& val ) const { val = get_imp()->peek(); } \
  GET_T peek( tlm::tlm_tag<GET_T> *t = 0 ) const { return get_imp()->peek(); }

#define UVM_BIDIR_NONBLOCKING_PUT_PORT_METHODS(PUT_T) \
  bool try_put(const PUT_T& val) { return get_imp()->nb_put(val); } \
  bool can_put() const { return get_imp()->nb_can_put(); } \
  bool nb_put(const PUT_T& val) { return get_imp()->nb_put(val); } \
  bool nb_can_put() const { return get_imp()->nb_can_put(); } \
  bool nb_can_put( tlm::tlm_tag<PUT_T> *t = 0 ) const { return get_imp()->nb_can_put(); } \
  const sc_core::sc_event& ok_to_put( tlm::tlm_tag<PUT_T> *t = 0 ) const { return get_imp()->ok_to_put(); }

#define UVM_BIDIR_NONBLOCKING_GET_PEEK_PORT_METHODS(GET_T) \
  bool try_get( GET_T& val ) { return get_imp()->nb_get(val); } \
  bool can_get() const { return get_imp()->nb_can_get(); } \
  bool nb_get( GET_T& val ) { return get_imp()->nb_get(val); } \
  bool nb_can_get() const { return get_imp()->nb_can_get(); } \
  bool nb_can_get( tlm::tlm_tag<GET_T> *t = 0 ) const { return get_imp()->nb_can_get(); } \
  const sc_core::sc_event& ok_to_get( tlm::tlm_tag<GET_T> *t = 0 ) const { return get_imp()->ok_to_get(); } \
  bool try_peek( GET_T& val ) const { return get_imp()->nb_peek(val); } \
  bool can_peek() const { return get_imp()->nb_can_peek(); } \
  bool nb_peek( GET_T& val ) const { return get_imp()->nb_peek(val); } \
  bool nb_can_peek() const { return get_imp()->nb_can_peek(); } \
  bool nb_can_peek( tlm::tlm_tag<GET_T> *t = 0 ) const { return get_imp()->nb_can_peek(); } \
  const sc_core::sc_event& ok_to_peek( tlm::tlm_tag<GET_T> *t = 0 ) const { return get_imp()->ok_to_peek(); }

//---------------------------------------------------------------------------
// TLM1 transport ports
//---------------------------------------------------------------------------

//! The port uvm_blocking_transport_port requires the blocking interface
//! tlm::tlm_transport_if<REQ, RSP> to transport a request and its response in one call.

template <typename REQ, typename RSP = REQ>
class uvm_blocking_transport_port
: public uvm_port_base< tlm::tlm_transport_if<REQ, RSP> >
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_transport_if<REQ, RSP> m_if_type;

 public:
  uvm_blocking_transport_port() :
    uvm_port_base< tlm::tlm_transport_if<REQ, RSP> >() {}

  uvm_blocking_transport_port( const std::string& name ) :
    uvm_port_base< tlm::tlm_transport_if<REQ, RSP> >( name ) {}

  UVM_PORT_COMMON(m_if_type)
  UVM_BLOCKING_TRANSPORT_PORT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_blocking_transport_port";
  }

  virtual ~uvm_blocking_transport_port() {}
};

//! The port uvm_nonblocking_transport_port requires the non-blocking interface
//! uvm_tlm_nonblocking_transport_if<REQ, RSP> to transport a request and its response in one call.

template <typename REQ, typename RSP = REQ>
class uvm_nonblocking_transport_port
: public uvm_port_base< uvm_tlm_nonblocking_transport_if<REQ, RSP> >
{
  // the interface type, as a single macro argument
  typedef uvm_tlm_nonblocking_transport_if<REQ, RSP> m_if_type;

 public:
  uvm_nonblocking_transport_port() :
    uvm_port_base< uvm_tlm_nonblocking_transport_if<REQ, RSP> >() {}

  uvm_nonblocking_transport_port( const std::string& name ) :
    uvm_port_base< uvm_tlm_nonblocking_transport_if<REQ, RSP> >( name ) {}

  UVM_PORT_COMMON(m_if_type)
  UVM_NONBLOCKING_TRANSPORT_PORT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_nonblocking_transport_port";
  }

  virtual ~uvm_nonblocking_transport_port() {}
};

//! The port uvm_transport_port requires the blocking and non-blocking interface
//! uvm_tlm_transport_if<REQ, RSP> to transport a request and its response in one call.

template <typename REQ, typename RSP = REQ>
class uvm_transport_port
: public uvm_port_base< uvm_tlm_transport_if<REQ, RSP> >
{
  // the interface type, as a single macro argument
  typedef uvm_tlm_transport_if<REQ, RSP> m_if_type;

 public:
  uvm_transport_port() :
    uvm_port_base< uvm_tlm_transport_if<REQ, RSP> >() {}

  uvm_transport_port( const std::string& name ) :
    uvm_port_base< uvm_tlm_transport_if<REQ, RSP> >( name ) {}

  UVM_PORT_COMMON(m_if_type)
  UVM_NONBLOCKING_TRANSPORT_PORT_METHODS
  UVM_BLOCKING_TRANSPORT_PORT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_transport_port";
  }

  virtual ~uvm_transport_port() {}
};

//---------------------------------------------------------------------------
// TLM1 master ports
//---------------------------------------------------------------------------

//! The port uvm_blocking_master_port requires the blocking interface
//! tlm::tlm_blocking_master_if<REQ, RSP> to put requests and get or peek responses.

template <typename REQ, typename RSP = REQ>
class uvm_blocking_master_port
: public uvm_port_base< tlm::tlm_blocking_master_if<REQ, RSP> >
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_blocking_master_if<REQ, RSP> m_if_type;

 public:
  uvm_blocking_master_port() :
    uvm_port_base< tlm::tlm_blocking_master_if<REQ, RSP> >() {}

  uvm_blocking_master_port( const std::string& name ) :
    uvm_port_base< tlm::tlm_blocking_master_if<REQ, RSP> >( name ) {}

  UVM_PORT_COMMON(m_if_type)
  UVM_BIDIR_BLOCKING_PUT_PORT_METHODS(REQ)
  UVM_BIDIR_BLOCKING_GET_PEEK_PORT_METHODS(RSP)

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_blocking_master_port";
  }

  virtual ~uvm_blocking_master_port() {}
};

//! The port uvm_nonblocking_master_port requires the non-blocking interface
//! tlm::tlm_nonblocking_master_if<REQ, RSP> to put requests and get or peek responses.

template <typename REQ, typename RSP = REQ>
class uvm_nonblocking_master_port
: public uvm_port_base< tlm::tlm_nonblocking_master_if<REQ, RSP> >
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_nonblocking_master_if<REQ, RSP> m_if_type;

 public:
  uvm_nonblocking_master_port() :
    uvm_port_base< tlm::tlm_nonblocking_master_if<REQ, RSP> >() {}

  uvm_nonblocking_master_port( const std::string& name ) :
    uvm_port_base< tlm::tlm_nonblocking_master_if<REQ, RSP> >( name ) {}

  UVM_PORT_COMMON(m_if_type)
  UVM_BIDIR_NONBLOCKING_PUT_PORT_METHODS(REQ)
  UVM_BIDIR_NONBLOCKING_GET_PEEK_PORT_METHODS(RSP)

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_nonblocking_master_port";
  }

  virtual ~uvm_nonblocking_master_port() {}
};

//! The port uvm_master_port requires the blocking and non-blocking interface
//! tlm::tlm_master_if<REQ, RSP> to put requests and get or peek responses.

template <typename REQ, typename RSP = REQ>
class uvm_master_port
: public uvm_port_base< tlm::tlm_master_if<REQ, RSP> >
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_master_if<REQ, RSP> m_if_type;

 public:
  uvm_master_port() :
    uvm_port_base< tlm::tlm_master_if<REQ, RSP> >() {}

  uvm_master_port( const std::string& name ) :
    uvm_port_base< tlm::tlm_master_if<REQ, RSP> >( name ) {}

  UVM_PORT_COMMON(m_if_type)
  UVM_BIDIR_NONBLOCKING_PUT_PORT_METHODS(REQ)
  UVM_BIDIR_NONBLOCKING_GET_PEEK_PORT_METHODS(RSP)
  UVM_BIDIR_BLOCKING_PUT_PORT_METHODS(REQ)
  UVM_BIDIR_BLOCKING_GET_PEEK_PORT_METHODS(RSP)

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_master_port";
  }

  virtual ~uvm_master_port() {}
};

//---------------------------------------------------------------------------
// TLM1 slave ports
//---------------------------------------------------------------------------

//! The port uvm_blocking_slave_port requires the blocking interface
//! tlm::tlm_blocking_slave_if<REQ, RSP> to get or peek requests and put responses.

template <typename REQ, typename RSP = REQ>
class uvm_blocking_slave_port
: public uvm_port_base< tlm::tlm_blocking_slave_if<REQ, RSP> >
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_blocking_slave_if<REQ, RSP> m_if_type;

 public:
  uvm_blocking_slave_port() :
    uvm_port_base< tlm::tlm_blocking_slave_if<REQ, RSP> >() {}

  uvm_blocking_slave_port( const std::string& name ) :
    uvm_port_base< tlm::tlm_blocking_slave_if<REQ, RSP> >( name ) {}

  UVM_PORT_COMMON(m_if_type)
  UVM_BIDIR_BLOCKING_PUT_PORT_METHODS(RSP)
  UVM_BIDIR_BLOCKING_GET_PEEK_PORT_METHODS(REQ)

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_blocking_slave_port";
  }

  virtual ~uvm_blocking_slave_port() {}
};

//! The port uvm_nonblocking_slave_port requires the non-blocking interface
//! tlm::tlm_nonblocking_slave_if<REQ, RSP> to get or peek requests and put responses.

template <typename REQ, typename RSP = REQ>
class uvm_nonblocking_slave_port
: public uvm_port_base< tlm::tlm_nonblocking_slave_if<REQ, RSP> >
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_nonblocking_slave_if<REQ, RSP> m_if_type;

 public:
  uvm_nonblocking_slave_port() :
    uvm_port_base< tlm::tlm_nonblocking_slave_if<REQ, RSP> >() {}

  uvm_nonblocking_slave_port( const std::string& name ) :
    uvm_port_base< tlm::tlm_nonblocking_slave_if<REQ, RSP> >( name ) {}

  UVM_PORT_COMMON(m_if_type)
  UVM_BIDIR_NONBLOCKING_PUT_PORT_METHODS(RSP)
  UVM_BIDIR_NONBLOCKING_GET_PEEK_PORT_METHODS(REQ)

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_nonblocking_slave_port";
  }

  virtual ~uvm_nonblocking_slave_port() {}
};

//! The port uvm_slave_port requires the blocking and non-blocking interface
//! tlm::tlm_slave_if<REQ, RSP> to get or peek requests and put responses.

template <typename REQ, typename RSP = REQ>
class uvm_slave_port
: public uvm_port_base< tlm::tlm_slave_if<REQ, RSP> >
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_slave_if<REQ, RSP> m_if_type;

 public:
  uvm_slave_port() :
    uvm_port_base< tlm::tlm_slave_if<REQ, RSP> >() {}

  uvm_slave_port( const std::string& name ) :
    uvm_port_base< tlm::tlm_slave_if<REQ, RSP> >( name ) {}

  UVM_PORT_COMMON(m_if_type)
  UVM_BIDIR_NONBLOCKING_PUT_PORT_METHODS(RSP)
  UVM_BIDIR_NONBLOCKING_GET_PEEK_PORT_METHODS(REQ)
  UVM_BIDIR_BLOCKING_PUT_PORT_METHODS(RSP)
  UVM_BIDIR_BLOCKING_GET_PEEK_PORT_METHODS(REQ)

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_slave_port";
  }

  virtual ~uvm_slave_port() {}
};

} // namespace uvm

#endif // UVM_BIDIR_PORTS_H_
