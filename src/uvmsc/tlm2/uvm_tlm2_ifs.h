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

#ifndef UVM_TLM2_IFS_H_
#define UVM_TLM2_IFS_H_

#include <systemc>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/tlm2/uvm_tlm2_defines.h"
#include "uvmsc/tlm2/uvm_tlm_gp.h"
#include "uvmsc/tlm2/uvm_tlm_time.h"

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_tlm_if_base<T, P>
//
//! Base transport interface for IEEE 1800.2-2020 section 12.4.1 TLM-2
//! blocking and non-blocking sockets.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload, typename P = uvm_tlm_phase_e>
class uvm_tlm_if_base : public virtual sc_core::sc_interface
{
 public:
  virtual ~uvm_tlm_if_base() {}

  virtual void b_transport( T& t, uvm_tlm_time& delay )
  {
    (void)t;
    (void)delay;
    UVM_ERROR("UVM/TLM2/UNIMP", "b_transport not implemented on this TLM-2 interface.");
  }

  virtual uvm_tlm_sync_e nb_transport_fw( T& t, P& p, uvm_tlm_time& delay )
  {
    (void)t;
    (void)p;
    (void)delay;
    UVM_ERROR("UVM/TLM2/UNIMP", "nb_transport_fw not implemented on this TLM-2 interface.");
    return UVM_TLM_COMPLETED;
  }

  virtual uvm_tlm_sync_e nb_transport_bw( T& t, P& p, uvm_tlm_time& delay )
  {
    (void)t;
    (void)p;
    (void)delay;
    UVM_ERROR("UVM/TLM2/UNIMP", "nb_transport_bw not implemented on this TLM-2 interface.");
    return UVM_TLM_COMPLETED;
  }
};

//----------------------------------------------------------------------
// Class: uvm_tlm_if<T, P>
//
//! Convenience alias deriving virtually from uvm_tlm_if_base<T, P>.
//----------------------------------------------------------------------

template <typename T = uvm_tlm_generic_payload, typename P = uvm_tlm_phase_e>
class uvm_tlm_if : public virtual uvm_tlm_if_base<T, P>
{
 public:
  virtual ~uvm_tlm_if() {}
};

} // namespace uvm

#endif // UVM_TLM2_IFS_H_
