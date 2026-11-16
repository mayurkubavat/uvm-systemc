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

#ifndef UVM_BIDIR_IMPS_H_
#define UVM_BIDIR_IMPS_H_

#include <systemc>
#include <tlm>

#include <string>

#include "uvmsc/base/uvm_export_base.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/tlm1/uvm_imps.h"
#include "uvmsc/tlm1/uvm_tlm_transport_ifs.h"

namespace uvm {

//---------------------------------------------------------------------------
// macros for the bidirectional imp implementations
//
// The UVM put/get/peek/transport API has been implemented, as well as the
// SystemC equivalent functions for compatibility reasons.
//
// A transport imp calls the member functions
//   void transport( const REQ& req, RSP& rsp )
//   bool nb_transport( const REQ& req, RSP& rsp )
// of its implementation. A master or slave imp calls the same member
// functions of the request and response implementations as the put, get
// and peek imps do: put, try_put and can_put for transactions of type
// PUT_T, and get, peek, try_get, can_get, try_peek and can_peek for
// transactions of type GET_T.
//---------------------------------------------------------------------------

#define UVM_BLOCKING_TRANSPORT_IMP_METHODS \
  void transport( const REQ& req, RSP& rsp ) { m_imp->transport(req, rsp); } \
  RSP transport( const REQ& req ) { RSP rsp; m_imp->transport(req, rsp); return rsp; }

#define UVM_NONBLOCKING_TRANSPORT_IMP_METHODS \
  bool nb_transport( const REQ& req, RSP& rsp ) { return m_imp->nb_transport(req, rsp); }

#define UVM_BIDIR_BLOCKING_PUT_IMP_METHODS(PUT_T, IMP) \
  void put( const PUT_T& val ) { IMP->put(val); }

#define UVM_BIDIR_BLOCKING_GET_PEEK_IMP_METHODS(GET_T, IMP) \
  void get( GET_T& val )  { val = IMP->get(); } \
  GET_T get( tlm::tlm_tag<GET_T> *t = 0 ) { return IMP->get(); } \
  void peek( GET_T& val ) const { val = IMP->peek(); } \
  GET_T peek( tlm::tlm_tag<GET_T> *t = 0 ) const { return IMP->peek(); }

#define UVM_BIDIR_NONBLOCKING_PUT_IMP_METHODS(PUT_T, IMP) \
  bool try_put(const PUT_T& val) { return IMP->try_put(val); } \
  bool can_put() { return IMP->can_put(); } \
  bool nb_put(const PUT_T& val) { return IMP->try_put(val); } \
  bool nb_can_put() { return IMP->can_put(); } \
  bool nb_can_put( tlm::tlm_tag<PUT_T> *t = 0 ) const { return IMP->can_put(); } \
  const sc_core::sc_event& ok_to_put( tlm::tlm_tag<PUT_T> *t = 0 ) const { return this->e; /* non-functional in uvmsc */ }

#define UVM_BIDIR_NONBLOCKING_GET_PEEK_IMP_METHODS(GET_T, IMP) \
  bool try_get( GET_T& val ) { return IMP->try_get(val); } \
  bool can_get() { return IMP->can_get(); } \
  bool nb_get( GET_T& val ) { return IMP->try_get(val); } \
  bool nb_can_get() { return IMP->can_get(); } \
  bool nb_can_get( tlm::tlm_tag<GET_T> *t = 0 ) const { return IMP->can_get(); } \
  const sc_core::sc_event& ok_to_get( tlm::tlm_tag<GET_T> *t = 0 ) const { return this->e; /* non-functional in uvmsc */ } \
  bool try_peek( GET_T& val ) const { return IMP->try_peek(val); } \
  bool can_peek() const { return IMP->can_peek(); } \
  bool nb_peek( GET_T& val ) const { return IMP->try_peek(val); } \
  bool nb_can_peek() const { return IMP->can_peek(); } \
  bool nb_can_peek( tlm::tlm_tag<GET_T> *t = 0 ) const { return IMP->can_peek(); } \
  const sc_core::sc_event& ok_to_peek( tlm::tlm_tag<GET_T> *t = 0 ) const { return this->e; /* non-functional in uvmsc */ }

//---------------------------------------------------------------------------
// Implementation defined: returns the request or response implementation
// of a master or slave imp, if none is given: the implementation imp
// itself, if it is of the same type, or else imp converted with
// dynamic_cast.
//---------------------------------------------------------------------------

template <typename TO, typename FROM>
struct uvm_bidir_imp_cast
{
  static TO* cast( FROM* imp ) { return dynamic_cast<TO*>(imp); }
};

template <typename T>
struct uvm_bidir_imp_cast<T, T>
{
  static T* cast( T* imp ) { return imp; }
};

//---------------------------------------------------------------------------
// TLM1 transport implementation ports (imps)
//---------------------------------------------------------------------------

//! The imp uvm_blocking_transport_imp provides the blocking interface
//! tlm::tlm_transport_if<REQ, RSP> to transport a request and its response in one call, by calling the member functions of
//! the implementation imp.

template <typename REQ = int, typename RSP = REQ, typename IMP = int>
class uvm_blocking_transport_imp
: public uvm_export_base< tlm::tlm_transport_if<REQ, RSP> >,
  public virtual tlm::tlm_transport_if<REQ, RSP>
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_transport_if<REQ, RSP> m_if_type;

 public:
  uvm_blocking_transport_imp( IMP* imp ) :
    uvm_export_base< tlm::tlm_transport_if<REQ, RSP> >(), m_imp(imp)
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  uvm_blocking_transport_imp( const std::string& name, IMP* imp ) :
    uvm_export_base< tlm::tlm_transport_if<REQ, RSP> >( name ), m_imp(imp)
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  UVM_BLOCKING_TRANSPORT_IMP_METHODS

  UVM_NO_BIND_AND_CONNECT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_blocking_transport_imp";
  }

  virtual ~uvm_blocking_transport_imp() {}

 private:
  IMP* m_imp;
};

//! The imp uvm_nonblocking_transport_imp provides the non-blocking interface
//! uvm_tlm_nonblocking_transport_if<REQ, RSP> to transport a request and its response in one call, by calling the member functions of
//! the implementation imp.

template <typename REQ = int, typename RSP = REQ, typename IMP = int>
class uvm_nonblocking_transport_imp
: public uvm_export_base< uvm_tlm_nonblocking_transport_if<REQ, RSP> >,
  public virtual uvm_tlm_nonblocking_transport_if<REQ, RSP>
{
  // the interface type, as a single macro argument
  typedef uvm_tlm_nonblocking_transport_if<REQ, RSP> m_if_type;

 public:
  uvm_nonblocking_transport_imp( IMP* imp ) :
    uvm_export_base< uvm_tlm_nonblocking_transport_if<REQ, RSP> >(), m_imp(imp)
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  uvm_nonblocking_transport_imp( const std::string& name, IMP* imp ) :
    uvm_export_base< uvm_tlm_nonblocking_transport_if<REQ, RSP> >( name ), m_imp(imp)
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  UVM_NONBLOCKING_TRANSPORT_IMP_METHODS

  UVM_NO_BIND_AND_CONNECT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_nonblocking_transport_imp";
  }

  virtual ~uvm_nonblocking_transport_imp() {}

 private:
  IMP* m_imp;
};

//! The imp uvm_transport_imp provides the blocking and non-blocking interface
//! uvm_tlm_transport_if<REQ, RSP> to transport a request and its response in one call, by calling the member functions of
//! the implementation imp.

template <typename REQ = int, typename RSP = REQ, typename IMP = int>
class uvm_transport_imp
: public uvm_export_base< uvm_tlm_transport_if<REQ, RSP> >,
  public virtual uvm_tlm_transport_if<REQ, RSP>
{
  // the interface type, as a single macro argument
  typedef uvm_tlm_transport_if<REQ, RSP> m_if_type;

 public:
  uvm_transport_imp( IMP* imp ) :
    uvm_export_base< uvm_tlm_transport_if<REQ, RSP> >(), m_imp(imp)
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  uvm_transport_imp( const std::string& name, IMP* imp ) :
    uvm_export_base< uvm_tlm_transport_if<REQ, RSP> >( name ), m_imp(imp)
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  UVM_NONBLOCKING_TRANSPORT_IMP_METHODS
  UVM_BLOCKING_TRANSPORT_IMP_METHODS

  UVM_NO_BIND_AND_CONNECT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_transport_imp";
  }

  virtual ~uvm_transport_imp() {}

 private:
  IMP* m_imp;
};

//---------------------------------------------------------------------------
// TLM1 master implementation ports (imps)
//---------------------------------------------------------------------------

//! The imp uvm_blocking_master_imp provides the blocking interface
//! tlm::tlm_blocking_master_if<REQ, RSP> to put requests and get or peek responses. The requests are passed to and from
//! the implementation req_imp, the responses to and from the
//! implementation rsp_imp. If not given, these are the implementation imp.

template <typename REQ = int, typename RSP = REQ, typename IMP = int,
          typename REQ_IMP = IMP, typename RSP_IMP = IMP>
class uvm_blocking_master_imp
: public uvm_export_base< tlm::tlm_blocking_master_if<REQ, RSP> >,
  public virtual tlm::tlm_blocking_master_if<REQ, RSP>
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_blocking_master_if<REQ, RSP> m_if_type;

 public:
  uvm_blocking_master_imp( IMP* imp, REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_blocking_master_if<REQ, RSP> >(),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  uvm_blocking_master_imp( const std::string& name, IMP* imp,
                           REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_blocking_master_if<REQ, RSP> >( name ),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  UVM_BIDIR_BLOCKING_PUT_IMP_METHODS(REQ, m_req_imp)
  UVM_BIDIR_BLOCKING_GET_PEEK_IMP_METHODS(RSP, m_rsp_imp)

  UVM_NO_BIND_AND_CONNECT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_blocking_master_imp";
  }

  virtual ~uvm_blocking_master_imp() {}

 private:
  REQ_IMP* m_req_imp;
  RSP_IMP* m_rsp_imp;
};

//! The imp uvm_nonblocking_master_imp provides the non-blocking interface
//! tlm::tlm_nonblocking_master_if<REQ, RSP> to put requests and get or peek responses. The requests are passed to and from
//! the implementation req_imp, the responses to and from the
//! implementation rsp_imp. If not given, these are the implementation imp.

template <typename REQ = int, typename RSP = REQ, typename IMP = int,
          typename REQ_IMP = IMP, typename RSP_IMP = IMP>
class uvm_nonblocking_master_imp
: public uvm_export_base< tlm::tlm_nonblocking_master_if<REQ, RSP> >,
  public virtual tlm::tlm_nonblocking_master_if<REQ, RSP>
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_nonblocking_master_if<REQ, RSP> m_if_type;

 public:
  uvm_nonblocking_master_imp( IMP* imp, REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_nonblocking_master_if<REQ, RSP> >(),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  uvm_nonblocking_master_imp( const std::string& name, IMP* imp,
                              REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_nonblocking_master_if<REQ, RSP> >( name ),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  UVM_BIDIR_NONBLOCKING_PUT_IMP_METHODS(REQ, m_req_imp)
  UVM_BIDIR_NONBLOCKING_GET_PEEK_IMP_METHODS(RSP, m_rsp_imp)

  UVM_NO_BIND_AND_CONNECT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_nonblocking_master_imp";
  }

  virtual ~uvm_nonblocking_master_imp() {}

 private:
  REQ_IMP* m_req_imp;
  RSP_IMP* m_rsp_imp;
  sc_core::sc_event e; /* non-functional in uvmsc */
};

//! The imp uvm_master_imp provides the blocking and non-blocking interface
//! tlm::tlm_master_if<REQ, RSP> to put requests and get or peek responses. The requests are passed to and from
//! the implementation req_imp, the responses to and from the
//! implementation rsp_imp. If not given, these are the implementation imp.

template <typename REQ = int, typename RSP = REQ, typename IMP = int,
          typename REQ_IMP = IMP, typename RSP_IMP = IMP>
class uvm_master_imp
: public uvm_export_base< tlm::tlm_master_if<REQ, RSP> >,
  public virtual tlm::tlm_master_if<REQ, RSP>
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_master_if<REQ, RSP> m_if_type;

 public:
  uvm_master_imp( IMP* imp, REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_master_if<REQ, RSP> >(),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  uvm_master_imp( const std::string& name, IMP* imp,
                  REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_master_if<REQ, RSP> >( name ),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  UVM_BIDIR_NONBLOCKING_PUT_IMP_METHODS(REQ, m_req_imp)
  UVM_BIDIR_NONBLOCKING_GET_PEEK_IMP_METHODS(RSP, m_rsp_imp)
  UVM_BIDIR_BLOCKING_PUT_IMP_METHODS(REQ, m_req_imp)
  UVM_BIDIR_BLOCKING_GET_PEEK_IMP_METHODS(RSP, m_rsp_imp)

  UVM_NO_BIND_AND_CONNECT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_master_imp";
  }

  virtual ~uvm_master_imp() {}

 private:
  REQ_IMP* m_req_imp;
  RSP_IMP* m_rsp_imp;
  sc_core::sc_event e; /* non-functional in uvmsc */
};

//---------------------------------------------------------------------------
// TLM1 slave implementation ports (imps)
//---------------------------------------------------------------------------

//! The imp uvm_blocking_slave_imp provides the blocking interface
//! tlm::tlm_blocking_slave_if<REQ, RSP> to get or peek requests and put responses. The requests are passed to and from
//! the implementation req_imp, the responses to and from the
//! implementation rsp_imp. If not given, these are the implementation imp.

template <typename REQ = int, typename RSP = REQ, typename IMP = int,
          typename REQ_IMP = IMP, typename RSP_IMP = IMP>
class uvm_blocking_slave_imp
: public uvm_export_base< tlm::tlm_blocking_slave_if<REQ, RSP> >,
  public virtual tlm::tlm_blocking_slave_if<REQ, RSP>
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_blocking_slave_if<REQ, RSP> m_if_type;

 public:
  uvm_blocking_slave_imp( IMP* imp, REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_blocking_slave_if<REQ, RSP> >(),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  uvm_blocking_slave_imp( const std::string& name, IMP* imp,
                          REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_blocking_slave_if<REQ, RSP> >( name ),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  UVM_BIDIR_BLOCKING_PUT_IMP_METHODS(RSP, m_rsp_imp)
  UVM_BIDIR_BLOCKING_GET_PEEK_IMP_METHODS(REQ, m_req_imp)

  UVM_NO_BIND_AND_CONNECT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_blocking_slave_imp";
  }

  virtual ~uvm_blocking_slave_imp() {}

 private:
  REQ_IMP* m_req_imp;
  RSP_IMP* m_rsp_imp;
};

//! The imp uvm_nonblocking_slave_imp provides the non-blocking interface
//! tlm::tlm_nonblocking_slave_if<REQ, RSP> to get or peek requests and put responses. The requests are passed to and from
//! the implementation req_imp, the responses to and from the
//! implementation rsp_imp. If not given, these are the implementation imp.

template <typename REQ = int, typename RSP = REQ, typename IMP = int,
          typename REQ_IMP = IMP, typename RSP_IMP = IMP>
class uvm_nonblocking_slave_imp
: public uvm_export_base< tlm::tlm_nonblocking_slave_if<REQ, RSP> >,
  public virtual tlm::tlm_nonblocking_slave_if<REQ, RSP>
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_nonblocking_slave_if<REQ, RSP> m_if_type;

 public:
  uvm_nonblocking_slave_imp( IMP* imp, REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_nonblocking_slave_if<REQ, RSP> >(),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  uvm_nonblocking_slave_imp( const std::string& name, IMP* imp,
                             REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_nonblocking_slave_if<REQ, RSP> >( name ),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  UVM_BIDIR_NONBLOCKING_PUT_IMP_METHODS(RSP, m_rsp_imp)
  UVM_BIDIR_NONBLOCKING_GET_PEEK_IMP_METHODS(REQ, m_req_imp)

  UVM_NO_BIND_AND_CONNECT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_nonblocking_slave_imp";
  }

  virtual ~uvm_nonblocking_slave_imp() {}

 private:
  REQ_IMP* m_req_imp;
  RSP_IMP* m_rsp_imp;
  sc_core::sc_event e; /* non-functional in uvmsc */
};

//! The imp uvm_slave_imp provides the blocking and non-blocking interface
//! tlm::tlm_slave_if<REQ, RSP> to get or peek requests and put responses. The requests are passed to and from
//! the implementation req_imp, the responses to and from the
//! implementation rsp_imp. If not given, these are the implementation imp.

template <typename REQ = int, typename RSP = REQ, typename IMP = int,
          typename REQ_IMP = IMP, typename RSP_IMP = IMP>
class uvm_slave_imp
: public uvm_export_base< tlm::tlm_slave_if<REQ, RSP> >,
  public virtual tlm::tlm_slave_if<REQ, RSP>
{
  // the interface type, as a single macro argument
  typedef tlm::tlm_slave_if<REQ, RSP> m_if_type;

 public:
  uvm_slave_imp( IMP* imp, REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_slave_if<REQ, RSP> >(),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  uvm_slave_imp( const std::string& name, IMP* imp,
                 REQ_IMP* req_imp = nullptr, RSP_IMP* rsp_imp = nullptr ) :
    uvm_export_base< tlm::tlm_slave_if<REQ, RSP> >( name ),
    m_req_imp(req_imp != nullptr ? req_imp : uvm_bidir_imp_cast<REQ_IMP, IMP>::cast(imp)),
    m_rsp_imp(rsp_imp != nullptr ? rsp_imp : uvm_bidir_imp_cast<RSP_IMP, IMP>::cast(imp))
  {
    UVM_BIND_TO_INTERFACE(m_if_type)
  }

  UVM_BIDIR_NONBLOCKING_PUT_IMP_METHODS(RSP, m_rsp_imp)
  UVM_BIDIR_NONBLOCKING_GET_PEEK_IMP_METHODS(REQ, m_req_imp)
  UVM_BIDIR_BLOCKING_PUT_IMP_METHODS(RSP, m_rsp_imp)
  UVM_BIDIR_BLOCKING_GET_PEEK_IMP_METHODS(REQ, m_req_imp)

  UVM_NO_BIND_AND_CONNECT_METHODS

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_slave_imp";
  }

  virtual ~uvm_slave_imp() {}

 private:
  REQ_IMP* m_req_imp;
  RSP_IMP* m_rsp_imp;
  sc_core::sc_event e; /* non-functional in uvmsc */
};

} // namespace uvm

#endif // UVM_BIDIR_IMPS_H_
