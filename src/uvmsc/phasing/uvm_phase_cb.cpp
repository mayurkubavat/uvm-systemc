//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2007-2014 Mentor Graphics Corporation
//   Copyright 2013-2024 NVIDIA Corporation
//   Copyright 2010-2014 Synopsys, Inc.
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

#include "uvmsc/phasing/uvm_phase_cb.h"
#include "uvmsc/phasing/uvm_phase.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_phase_state_change
//----------------------------------------------------------------------

uvm_phase_state_change::uvm_phase_state_change( const std::string& name )
: uvm_object(name), m_phase(nullptr), m_prev_state(UVM_PHASE_DORMANT),
  m_jump_to(nullptr)
{}

//----------------------------------------------------------------------
// member function: get_state (virtual)
//
//! Returns the state the phase just transitioned to.
//----------------------------------------------------------------------

uvm_phase_state uvm_phase_state_change::get_state() const
{
  return m_phase->get_state();
}

//----------------------------------------------------------------------
// member function: get_prev_state (virtual)
//
//! Returns the state the phase just transitioned from.
//----------------------------------------------------------------------

uvm_phase_state uvm_phase_state_change::get_prev_state() const
{
  return m_prev_state;
}

//----------------------------------------------------------------------
// member function: jump_to
//
//! Returns the phase that is the target of a phase jump, if the current
//! state is UVM_PHASE_ENDED or UVM_PHASE_JUMPING because of this jump.
//! Returns nullptr otherwise.
//----------------------------------------------------------------------

uvm_phase* uvm_phase_state_change::jump_to() const
{
  return m_jump_to;
}

const std::string uvm_phase_state_change::get_type_name() const
{
  return "uvm::uvm_phase_state_change";
}

uvm_phase_state_change::~uvm_phase_state_change()
{}

//----------------------------------------------------------------------
// CLASS: uvm_phase_cb
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Constructor
//
//! Registers the callback type for phases, so that a callback can be
//! added for all phases before any phase is used.
//----------------------------------------------------------------------

bool uvm_phase_cb::m_constructed = false;

uvm_phase_cb::uvm_phase_cb( const std::string& name )
: uvm_callback(name)
{
  m_constructed = true;
  uvm_callbacks<uvm_phase, uvm_phase_cb>::m_register_pair("uvm_phase", "uvm_phase_cb");
}

const std::string uvm_phase_cb::get_type_name() const
{
  return "uvm::uvm_phase_cb";
}

uvm_phase_cb::~uvm_phase_cb()
{}

//----------------------------------------------------------------------
// member function: m_exists (static)
//
//! Implementation defined
//! Returns true once a uvm_phase_cb has been constructed. Until then, no
//! callback can be registered, and the phases do not need to access the
//! callback pool, which would create the pool objects.
//----------------------------------------------------------------------

bool uvm_phase_cb::m_exists()
{
  return m_constructed;
}

} // namespace uvm
