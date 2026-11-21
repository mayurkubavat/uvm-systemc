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

#ifndef UVM_TLM2_DEFINES_H_
#define UVM_TLM2_DEFINES_H_

namespace uvm {

//----------------------------------------------------------------------
// Enum: uvm_tlm_command_e
//
//! Command attribute values matching SystemC tlm::tlm_command 1:1.
//----------------------------------------------------------------------

enum uvm_tlm_command_e
{
  UVM_TLM_READ_COMMAND   = 0,
  UVM_TLM_WRITE_COMMAND  = 1,
  UVM_TLM_IGNORE_COMMAND = 2
};

//----------------------------------------------------------------------
// Enum: uvm_tlm_response_status_e
//
//! Response status values matching SystemC tlm::tlm_response_status 1:1.
//----------------------------------------------------------------------

enum uvm_tlm_response_status_e
{
  UVM_TLM_OK_RESPONSE                =  1,
  UVM_TLM_INCOMPLETE_RESPONSE        =  0,
  UVM_TLM_GENERIC_ERROR_RESPONSE     = -1,
  UVM_TLM_ADDRESS_ERROR_RESPONSE     = -2,
  UVM_TLM_COMMAND_ERROR_RESPONSE     = -3,
  UVM_TLM_BURST_ERROR_RESPONSE       = -4,
  UVM_TLM_BYTE_ENABLE_ERROR_RESPONSE = -5
};

//----------------------------------------------------------------------
// Enum: uvm_tlm_gp_option_e
//
//! Generic payload option values matching SystemC tlm::tlm_gp_option 1:1.
//----------------------------------------------------------------------

enum uvm_tlm_gp_option_e
{
  UVM_TLM_MIN_PAYLOAD           = 0,
  UVM_TLM_FULL_PAYLOAD          = 1,
  UVM_TLM_FULL_PAYLOAD_ACCEPTED = 2
};

//----------------------------------------------------------------------
// Enum: uvm_tlm_phase_e
//
//! Non-blocking transport phases matching SystemC tlm::tlm_phase_enum 1:1.
//----------------------------------------------------------------------

enum uvm_tlm_phase_e
{
  UNINITIALIZED_PHASE = 0,
  BEGIN_REQ           = 1,
  END_REQ             = 2,
  BEGIN_RESP          = 3,
  END_RESP            = 4
};

//----------------------------------------------------------------------
// Enum: uvm_tlm_sync_e
//
//! Non-blocking transport synchronization values matching SystemC
//! tlm::tlm_sync_enum 1:1.
//----------------------------------------------------------------------

enum uvm_tlm_sync_e
{
  UVM_TLM_ACCEPTED  = 0,
  UVM_TLM_UPDATED   = 1,
  UVM_TLM_COMPLETED = 2
};

//----------------------------------------------------------------------
// Byte enable constants matching SystemC TLM_BYTE_DISABLED / TLM_BYTE_ENABLED
//----------------------------------------------------------------------

static constexpr unsigned char UVM_TLM_BYTE_DISABLED = 0x00;
static constexpr unsigned char UVM_TLM_BYTE_ENABLED  = 0xff;

} // namespace uvm

#endif // UVM_TLM2_DEFINES_H_
