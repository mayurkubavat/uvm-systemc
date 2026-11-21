//----------------------------------------------------------------------
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2010 Cadence Design Systems, Inc.
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

#ifndef UVM_PUSH_DRIVER_H_
#define UVM_PUSH_DRIVER_H_

#include <string>
#include <systemc>

#include "uvmsc/base/uvm_component_name.h"
#include "uvmsc/base/uvm_component.h"
#include "uvmsc/seq/uvm_sequence_item.h"
#include "uvmsc/tlm1/uvm_analysis_port.h"
#include "uvmsc/tlm1/uvm_imps.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_push_driver<REQ,RSP>
//
//! Base class for a driver that passively receives transactions, i.e.
//! does not initiate requests transactions. Also known as push mode.
//! Its ports are typically connected to the corresponding ports in a
//! push sequencer as follows:
//!
//!   push_sequencer.req_port.connect(push_driver.req_export);
//!   push_driver.rsp_port.connect(push_sequencer.rsp_export);
//!
//! The rsp_port needs connecting only if the driver will use it to
//! write responses to the analysis export in the sequencer.
//!
//! A derived class implements put(), which the sequencer calls with
//! each item; put() returns when the item is done.
//!
//! UVM-SV checks at the end of elaboration that req_export is connected.
//! Here req_export is an export bound to the driver itself, which does
//! not count its connections; an unconnected req_port of the sequencer is
//! reported by SystemC at elaboration.
//----------------------------------------------------------------------

template <typename REQ = uvm_sequence_item, typename RSP = REQ>
class uvm_push_driver : public uvm_component
{
 public:
  typedef uvm_push_driver<REQ, RSP> this_type;

  //! This export provides the blocking put interface whose default
  //! implementation produces an error. Derived drivers must override put()
  //! with an appropriate implementation (and not call the base put()).
  uvm_blocking_put_imp<REQ, this_type> req_export;

  //! This analysis port is used to send response transactions back to the
  //! originating sequencer.
  uvm_analysis_port<RSP> rsp_port;

  REQ req;
  RSP rsp;

  explicit uvm_push_driver( uvm_component_name name_ )
  : uvm_component(name_), req_export("req_export", this), rsp_port("rsp_port")
  {}

  virtual void put( const REQ& item )
  {
    uvm_report_fatal("UVM_PUSH_DRIVER",
      "Put task for push driver is not implemented", UVM_NONE);
  }

  virtual const std::string get_type_name() const
  {
    return std::string(this->kind());
  }

  virtual const char* kind() const // SystemC API
  {
    return "uvm::uvm_push_driver";
  }

}; // class uvm_push_driver

} // namespace uvm

#endif // UVM_PUSH_DRIVER_H_
