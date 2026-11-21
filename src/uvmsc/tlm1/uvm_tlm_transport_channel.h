//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2014-2024 NVIDIA Corporation
//   Copyright 2014 Semifore
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

#ifndef UVM_TLM_TRANSPORT_CHANNEL_H_
#define UVM_TLM_TRANSPORT_CHANNEL_H_

#include <systemc>
#include <tlm>

#include "uvmsc/base/uvm_component_name.h"
#include "uvmsc/tlm1/uvm_bidir_imps.h"
#include "uvmsc/tlm1/uvm_tlm_req_rsp_channel.h"

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_tlm_transport_channel<REQ,RSP>
//
//! The class uvm_tlm_transport_channel is a uvm_tlm_req_rsp_channel that
//! implements the transport interface. It is useful when modeling a
//! non-pipelined bus at the transaction level. Because the requests and
//! responses have a tightly coupled one-to-one relationship, the request
//! and response FIFO sizes are both set to one.
//!
//! A transaction passed via transport_export is put into the request
//! FIFO; the response is then taken from the response FIFO. A slave
//! gets the requests and puts the responses via the exports of the
//! uvm_tlm_req_rsp_channel, for example slave_export.
//----------------------------------------------------------------------

template <typename REQ, typename RSP = REQ>
class uvm_tlm_transport_channel : public uvm_tlm_req_rsp_channel<REQ, RSP>
{
 public:
  typedef uvm_tlm_transport_channel<REQ, RSP> this_type;

  //--------------------------------------------------------------------
  // Ports and exports
  //--------------------------------------------------------------------

  //! The export transport_export provides both the blocking and
  //! non-blocking transport interface member functions transport and
  //! nb_transport to the channel.
  uvm_transport_imp<REQ, RSP, this_type> transport_export;

  //--------------------------------------------------------------------
  // Constructors
  //--------------------------------------------------------------------

  //! The constructor shall create a new transport channel with request
  //! and response FIFOs of size one. If specified, the argument name
  //! shall define the name of the channel. Otherwise, the name of the
  //! channel is implementation-defined.
  uvm_tlm_transport_channel() :
    uvm_tlm_transport_channel(
      uvm_component_name(sc_core::sc_gen_unique_name("uvm_tlm_transport_channel")) )
  {}

  explicit uvm_tlm_transport_channel( uvm_component_name name ) :
    uvm_tlm_req_rsp_channel<REQ, RSP>(name, 1, 1),
    transport_export("transport_export", this)
  {}

  //--------------------------------------------------------------------
  // Member functions
  //--------------------------------------------------------------------

  //! Puts the request into the request FIFO and then gets the response
  //! from the response FIFO; blocks until both have completed.
  void transport( const REQ& req, RSP& rsp )
  {
    this->put_request_export->put(req);
    rsp = this->get_response_export->get();
  }

  //! Puts the request into the request FIFO and gets the response from
  //! the response FIFO, if this is possible without blocking. Returns
  //! true if both succeeded, false otherwise.
  bool nb_transport( const REQ& req, RSP& rsp )
  {
    if (!this->put_request_export->nb_put(req))
      return false;
    return this->get_response_export->nb_get(rsp);
  }

  virtual const char* kind() const
  {
    return "uvm::uvm_tlm_transport_channel";
  }

  virtual ~uvm_tlm_transport_channel() {}
}; // class uvm_tlm_transport_channel

} // namespace uvm

#endif // UVM_TLM_TRANSPORT_CHANNEL_H_
