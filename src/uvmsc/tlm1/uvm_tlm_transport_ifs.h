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

#ifndef UVM_TLM_TRANSPORT_IFS_H_
#define UVM_TLM_TRANSPORT_IFS_H_

#include <systemc>
#include <tlm>

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_tlm_nonblocking_transport_if<REQ,RSP>
//
//! The interface uvm_tlm_nonblocking_transport_if defines the
//! non-blocking transport member function nb_transport of the UVM TLM-1
//! transport interface. SystemC TLM-1 only defines the blocking
//! transport interface tlm::tlm_transport_if.
//----------------------------------------------------------------------

template <typename REQ, typename RSP = REQ>
class uvm_tlm_nonblocking_transport_if : public virtual sc_core::sc_interface
{
 public:
  //! Executes the given request and returns the response in rsp, if this
  //! is possible without blocking. Returns true if the transaction was
  //! executed, false otherwise.
  virtual bool nb_transport( const REQ& req, RSP& rsp ) = 0;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_transport_if<REQ,RSP>
//
//! The interface uvm_tlm_transport_if combines the blocking transport
//! interface tlm::tlm_transport_if and the non-blocking transport
//! interface uvm_tlm_nonblocking_transport_if.
//----------------------------------------------------------------------

template <typename REQ, typename RSP = REQ>
class uvm_tlm_transport_if
: public virtual tlm::tlm_transport_if<REQ, RSP>,
  public virtual uvm_tlm_nonblocking_transport_if<REQ, RSP>
{};

} // namespace uvm

#endif // UVM_TLM_TRANSPORT_IFS_H_
