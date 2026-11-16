//----------------------------------------------------------------------
//   Copyright 2014-2015 NXP B.V.
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

#ifndef UVM_TLM_REQ_RSP_CHANNEL_H_
#define UVM_TLM_REQ_RSP_CHANNEL_H_

#include <systemc>
#include <tlm>
#include "uvmsc/base/uvm_component_name.h"
#include "uvmsc/tlm1/uvm_analysis_port.h"

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_tlm_req_rsp_put_imp
//
//! Implementation-defined helper class, which provides the put interface
//! of a request or response FIFO of the #uvm_tlm_req_rsp_channel. Each
//! transaction which is accepted by the FIFO via the member function put
//! or nb_put is subsequently written to the associated analysis port.
//! All other member functions are passed on to the FIFO.
//----------------------------------------------------------------------

template <typename T>
class uvm_tlm_req_rsp_put_imp : public virtual tlm::tlm_fifo_put_if<T>
{
 public:
  uvm_tlm_req_rsp_put_imp( tlm::tlm_fifo_put_if<T>& fifo,
                           tlm::tlm_analysis_if<T>& ap )
  : m_fifo(fifo), m_ap(ap)
  {}

  void put( const T& t )
  {
    m_fifo.put(t);
    m_ap.write(t);
  }

  bool nb_put( const T& t )
  {
    if (!m_fifo.nb_put(t))
      return false;
    m_ap.write(t);
    return true;
  }

  bool nb_can_put( tlm::tlm_tag<T>* t = 0 ) const
  {
    return m_fifo.nb_can_put(t);
  }

  const sc_core::sc_event& ok_to_put( tlm::tlm_tag<T>* t = 0 ) const
  {
    return m_fifo.ok_to_put(t);
  }

  int used() const { return m_fifo.used(); }
  int size() const { return m_fifo.size(); }
  void debug() const { m_fifo.debug(); }
  bool nb_peek( T& t, int n ) const { return m_fifo.nb_peek(t, n); }
  bool nb_poke( const T& t, int n = 0 ) { return m_fifo.nb_poke(t, n); }

 private:
  tlm::tlm_fifo_put_if<T>& m_fifo;
  tlm::tlm_analysis_if<T>& m_ap;
}; // class uvm_tlm_req_rsp_put_imp

//----------------------------------------------------------------------
// Class: uvm_tlm_req_rsp_channel<REQ,RSP>
//
//! The class #uvm_tlm_req_rsp_channel offers a convenience layer for UVM
//! users and is compatible with the SystemC tlm::tlm_req_rsp_channel, since
//! it is derived from this class. It offers some UVM additional capabilities
//! such as the analysis ports for request and response monitoring.
//!
//! The #uvm_tlm_req_rsp_channel contains a request FIFO of type REQ and
//! a response FIFO of type RSP. These FIFOs can be of any size. This channel is
//! particularly useful for dealing with pipelined protocols where the request
//! and response are not tightly coupled.
//!
//! Type parameters:
//!
//! REQ:         Type of the request transactions conveyed by this channel.
//! RSP:         Type of the response transactions conveyed by this channel.
//! REQ_CHANNEL: Type of TLM FIFO used for the request transactions
//! RSP_CHANNEL: Type of TLM FIFO used for the response transactions
//!
//! The ports and exports of this class hide the unnamed exports with the
//! same name of the base class tlm::tlm_req_rsp_channel. Transactions which
//! are put via the exports of the base class are not written to the
//! analysis ports.
//----------------------------------------------------------------------

template < typename REQ , typename RSP = REQ ,
     typename REQ_CHANNEL = tlm::tlm_fifo<REQ> ,
     typename RSP_CHANNEL = tlm::tlm_fifo<RSP> >
class uvm_tlm_req_rsp_channel : public tlm::tlm_req_rsp_channel<REQ,RSP,REQ_CHANNEL,RSP_CHANNEL>
{
  typedef tlm::tlm_req_rsp_channel<REQ,RSP,REQ_CHANNEL,RSP_CHANNEL> base_type;

 public:
  //--------------------------------------------------------------------
  // Ports and exports
  //--------------------------------------------------------------------

  //! The analysis port request_ap shall send the request transactions,
  //! which are passed via the member function put or nb_put (via any port
  //! connected to the export put_request_export), via its member function
  //! write, to all connected analysis exports and imps.
  uvm_analysis_port<REQ> request_ap;

  //! The analysis port response_ap shall send the response transactions,
  //! which are passed via the member function put or nb_put (via any port
  //! connected to the export put_response_export), via its member function
  //! write, to all connected analysis exports and imps.
  uvm_analysis_port<RSP> response_ap;

  //! The export put_request_export shall provide both the blocking and
  //! non-blocking put interface member functions to the request FIFO based
  //! on interface tlm::tlm_fifo_put_if, being member functions put, nb_put
  //! and nb_can_put.
  sc_core::sc_export< tlm::tlm_fifo_put_if<REQ> > put_request_export;

  //! The export put_response_export shall provide both the blocking and
  //! non-blocking put interface member functions to the response FIFO based
  //! on interface tlm::tlm_fifo_put_if, being put, nb_put and nb_can_put.
  sc_core::sc_export< tlm::tlm_fifo_put_if<RSP> > put_response_export;

  //! The export get_request_export shall provide both the blocking and
  //! non-blocking get and peek interface member functions to the request FIFO
  //! based on interface tlm::tlm_fifo_get_if, being get, nb_get, nb_can_get,
  //! peek, nb_peek and nb_can_peek. It is functionally equivalent to
  //! get_peek_request_export.
  sc_core::sc_export< tlm::tlm_fifo_get_if<REQ> > get_request_export;

  //! The export get_response_export shall provide both the blocking and
  //! non-blocking get and peek interface member functions to the response
  //! FIFO based on interface tlm::tlm_fifo_get_if, being get, nb_get,
  //! nb_can_get, peek, nb_peek and nb_can_peek. It is functionally
  //! equivalent to get_peek_response_export.
  sc_core::sc_export< tlm::tlm_fifo_get_if<RSP> > get_response_export;

  //! The export get_peek_request_export shall provide both the blocking and
  //! non-blocking get and peek interface member functions to the request FIFO
  //! based on interface tlm::tlm_fifo_get_if, being get, nb_get, nb_can_get,
  //! peek, nb_peek and nb_can_peek. It is functionally equivalent to
  //! get_request_export.
  sc_core::sc_export< tlm::tlm_fifo_get_if<REQ> > get_peek_request_export;

  //! The export get_peek_response_export shall provide both the blocking and
  //! non-blocking get and peek interface member functions to the response
  //! FIFO based on interface tlm::tlm_fifo_get_if, being get, nb_get,
  //! nb_can_get, peek, nb_peek and nb_can_peek. It is functionally
  //! equivalent to get_response_export.
  sc_core::sc_export< tlm::tlm_fifo_get_if<RSP> > get_peek_response_export;

  //! The export master_export shall provide a single interface that allows
  //! a master to put requests and get or peek responses. It is a combination
  //! of the functionality offered by the exports put_request_export and
  //! get_peek_response_export.
  sc_core::sc_export< tlm::tlm_master_if<REQ, RSP> > master_export;

  //! The export slave_export shall provide a single interface that allows
  //! a slave to get or peek requests and to put responses. It is a
  //! combination of the functionality offered by the exports
  //! get_peek_request_export and put_response_export.
  sc_core::sc_export< tlm::tlm_slave_if<REQ, RSP> > slave_export;

  //--------------------------------------------------------------------
  // Constructors
  //--------------------------------------------------------------------

  //! The constructor shall create a new TLM-1 interface containing a request
  //! and response FIFO. The argument req_size specifies the size of the
  //! request FIFO. The argument rsp_size specifies the size of the response
  //! FIFO. If not specified, default size of these FIFOs is 1. If specified,
  //! the argument name shall define the name of the interface. Otherwise,
  //! the name of the interface is implementation-defined.
  uvm_tlm_req_rsp_channel( int req_size = 1 , int rsp_size = 1 ) :
    uvm_tlm_req_rsp_channel(
      sc_core::sc_module_name(sc_core::sc_gen_unique_name("uvm_tlm_req_rsp_channel")),
      req_size, rsp_size, 0 )
  {}

  uvm_tlm_req_rsp_channel( uvm_component_name name,
                           int req_size = 1, int rsp_size = 1 ) :
    uvm_tlm_req_rsp_channel( name.name(), req_size, rsp_size, 0 )
  {}

 private:
  // The module name object is kept alive by the delegating constructors
  // until the members below have been created, so that these become
  // children of this channel.
  uvm_tlm_req_rsp_channel( const sc_core::sc_module_name& name,
                           int req_size, int rsp_size, int ) :
    base_type(name, req_size, rsp_size),
    request_ap("request_ap"),
    response_ap("response_ap"),
    put_request_export("put_request_export"),
    put_response_export("put_response_export"),
    get_request_export("get_request_export"),
    get_response_export("get_response_export"),
    get_peek_request_export("get_peek_request_export"),
    get_peek_response_export("get_peek_response_export"),
    master_export("master_export"),
    slave_export("slave_export"),
    m_put_request(this->request_fifo, request_ap),
    m_put_response(this->response_fifo, response_ap),
    m_master(m_put_request, this->response_fifo),
    m_slave(this->request_fifo, m_put_response)
  {
    put_request_export.bind(m_put_request);
    put_response_export.bind(m_put_response);
    get_request_export.bind(this->request_fifo);
    get_response_export.bind(this->response_fifo);
    get_peek_request_export.bind(this->request_fifo);
    get_peek_response_export.bind(this->response_fifo);
    master_export.bind(m_master);
    slave_export.bind(m_slave);
  }

  uvm_tlm_req_rsp_put_imp<REQ> m_put_request;
  uvm_tlm_req_rsp_put_imp<RSP> m_put_response;
  tlm::tlm_master_imp<REQ, RSP> m_master;
  tlm::tlm_slave_imp<REQ, RSP> m_slave;
};


/////////////////////////////////////////////

} /* namespace uvm */

#endif /* UVM_TLM_REQ_RSP_CHANNEL_H_ */
