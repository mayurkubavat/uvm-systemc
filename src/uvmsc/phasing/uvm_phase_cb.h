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

#ifndef UVM_PHASE_CB_H_
#define UVM_PHASE_CB_H_

#include <string>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_object_globals.h"
#include "uvmsc/cb/uvm_callback.h"
#include "uvmsc/cb/uvm_callbacks.h"

namespace uvm {

// forward class declarations
class uvm_phase;

//----------------------------------------------------------------------
// CLASS: uvm_phase_state_change
//
//! Phase state transition descriptor. Used to describe the phase
//! transition that caused a uvm_phase_cb::phase_state_change callback to
//! be invoked.
//----------------------------------------------------------------------

class uvm_phase_state_change : public uvm_object
{
 public:
  friend class uvm_phase;

  explicit uvm_phase_state_change( const std::string& name = "uvm_phase_state_change" );

  //! Returns the state the phase just transitioned to. Functionally
  //! equivalent to uvm_phase::get_state.
  virtual uvm_phase_state get_state() const;

  //! Returns the state the phase just transitioned from.
  virtual uvm_phase_state get_prev_state() const;

  //! If the current state is UVM_PHASE_ENDED or UVM_PHASE_JUMPING because
  //! of a phase jump, returns the phase that is the target of jump.
  //! Returns nullptr otherwise.
  uvm_phase* jump_to() const;

  virtual const std::string get_type_name() const;

  virtual ~uvm_phase_state_change();

 private:
  uvm_phase* m_phase;
  uvm_phase_state m_prev_state;
  uvm_phase* m_jump_to;
}; // class uvm_phase_state_change

//----------------------------------------------------------------------
// CLASS: uvm_phase_cb
//
//! This class defines a callback member function that is invoked by the
//! phaser during the execution of a specific node in the phase graph or
//! all phase nodes. User-defined callback extensions can be used to
//! integrate data types that are not natively phase-aware with the UVM
//! phasing.
//!
//! For example:
//!
//!   class my_phase_cb : public uvm::uvm_phase_cb
//!   {
//!    public:
//!     my_phase_cb( const std::string& name ) : uvm_phase_cb(name) {}
//!
//!     virtual void phase_state_change( uvm::uvm_phase* phase,
//!                                      uvm::uvm_phase_state_change* change );
//!   };
//!
//!   my_phase_cb* cb = new my_phase_cb("cb");
//!   uvm::uvm_phase_cb_pool::add(nullptr, cb); // all phase nodes
//----------------------------------------------------------------------

class uvm_phase_cb : public uvm_callback
{
 public:
  //! The constructor shall create a new callback with the given name.
  explicit uvm_phase_cb( const std::string& name = "unnamed-uvm_phase_cb" );

  //! Called whenever a phase changes state. The change argument describes
  //! the transition that was just completed. The callback member function
  //! is invoked immediately after the phase state has changed, but before
  //! the phase implementation is executed.
  //!
  //! An extension may interact with the phase, such as raising the phase
  //! objection to prolong the phase, in a manner that is consistent with
  //! the current phase state.
  //!
  //! By default, the callback member function does nothing. Unless
  //! otherwise specified, modifying the phase transition descriptor has
  //! no effect on the phasing schedule or execution.
  virtual void phase_state_change( uvm_phase* phase,
                                   uvm_phase_state_change* change )
  {}

  virtual const std::string get_type_name() const;

  virtual ~uvm_phase_cb();

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  // returns true once a uvm_phase_cb has been constructed; until then,
  // no callback can be registered and the phases skip the callbacks
  static bool m_exists();

 private:
  static bool m_constructed;
}; // class uvm_phase_cb

//----------------------------------------------------------------------
// Typedef: uvm_phase_cb_pool
//
//! Convenience type for the uvm_callbacks<uvm_phase, uvm_phase_cb> class.
//----------------------------------------------------------------------

typedef uvm_callbacks<uvm_phase, uvm_phase_cb> uvm_phase_cb_pool;

} // namespace uvm

#endif // UVM_PHASE_CB_H_
