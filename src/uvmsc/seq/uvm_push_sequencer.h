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

#ifndef UVM_PUSH_SEQUENCER_H_
#define UVM_PUSH_SEQUENCER_H_

#include <string>
#include <systemc>

#include "uvmsc/base/uvm_component_name.h"
#include "uvmsc/phasing/uvm_phase.h"
#include "uvmsc/seq/uvm_sequence_item.h"
#include "uvmsc/seq/uvm_sequencer_param_base.h"
#include "uvmsc/tlm1/uvm_ports.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_push_sequencer<REQ,RSP>
//
//! The uvm_push_sequencer actively pushes sequence items to its
//! req_port, which is typically connected to the req_export of a
//! uvm_push_driver. It selects the next item from its sequences, puts
//! it to the driver, and marks the item as done when the put returns.
//----------------------------------------------------------------------

template <typename REQ = uvm_sequence_item, typename RSP = REQ>
class uvm_push_sequencer : public uvm_sequencer_param_base<REQ, RSP>
{
 public:
  typedef uvm_push_sequencer<REQ, RSP> this_type;

  //! The push sequencer requires access to a blocking put interface.
  //! A continuous stream of sequence items are sent out this port, based
  //! on the list of available sequences loaded into this sequencer.
  uvm_blocking_put_port<REQ> req_port;

  explicit uvm_push_sequencer( uvm_component_name name_ )
  : uvm_sequencer_param_base<REQ, RSP>(name_), req_port("req_port")
  {}

  //! Selects the items of the sequences and pushes them to req_port.
  //! UVM-SV selects them with m_safe_select_item(); this sequencer
  //! selects them as the pull sequencer of UVM-SystemC does, with
  //! m_select_sequence() and its request FIFO.
  virtual void run_phase( uvm_phase& phase )
  {
    REQ t;

    while (true)
    {
      this->m_select_sequence();
      t = this->m_req_fifo.get();

      req_port->put(t);

      this->m_wait_for_item_sequence_id = t.get_sequence_id();
      this->m_wait_for_item_transaction_id = t.get_transaction_id();
      this->m_wait_for_item_sequence_ev.notify();

      this->grant_queued_locks();
    }
  }

  virtual const std::string get_type_name() const
  {
    return std::string(this->kind());
  }

  virtual const char* kind() const // SystemC API
  {
    return "uvm::uvm_push_sequencer";
  }

}; // class uvm_push_sequencer

} // namespace uvm

#endif // UVM_PUSH_SEQUENCER_H_
