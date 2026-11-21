//----------------------------------------------------------------------
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2011 Cadence Design Systems, Inc.
//   Copyright 2010-2011 Synopsys, Inc.
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

#ifndef UVM_OBJECTION_CALLBACK_H_
#define UVM_OBJECTION_CALLBACK_H_

#include <string>

#include "uvmsc/cb/uvm_callback.h"
#include "uvmsc/cb/uvm_callbacks.h"

namespace uvm {

// forward class declarations
class uvm_object;
class uvm_objection;

//----------------------------------------------------------------------
// CLASS: uvm_objection_callback
//
//! The class uvm_objection_callback is the callback type that defines
//! the callback implementations for an objection callback. A user uses
//! the callback type uvm_objection_cbs_t to add callbacks to specific
//! objections.
//!
//! For example:
//!
//!   class my_objection_cb : public uvm::uvm_objection_callback
//!   {
//!    public:
//!     my_objection_cb( const std::string& name ) : uvm_objection_callback(name) {}
//!
//!     virtual void raised( uvm_objection* objection, uvm_object* obj,
//!                          uvm_object* source_obj,
//!                          const std::string& description, int count );
//!   };
//!
//!   my_objection_cb* cb = new my_objection_cb("cb");
//!   uvm::uvm_objection_cbs_t::add(nullptr, cb); // typewide callback
//----------------------------------------------------------------------

class uvm_objection_callback : public uvm_callback
{
 public:
  explicit uvm_objection_callback( const std::string& name = "uvm_objection_callback" );

  //! Objection raised callback function. Called by uvm_objection::raised.
  virtual void raised( uvm_objection* objection,
                       uvm_object* obj,
                       uvm_object* source_obj,
                       const std::string& description,
                       int count )
  {}

  //! Objection dropped callback function. Called by uvm_objection::dropped.
  virtual void dropped( uvm_objection* objection,
                        uvm_object* obj,
                        uvm_object* source_obj,
                        const std::string& description,
                        int count )
  {}

  //! Objection all_dropped callback function. Called by
  //! uvm_objection::all_dropped. It may consume time.
  virtual void all_dropped( uvm_objection* objection,
                            uvm_object* obj,
                            uvm_object* source_obj,
                            const std::string& description,
                            int count )
  {}

}; // class uvm_objection_callback

//----------------------------------------------------------------------
// Typedef: uvm_objection_cbs_t
//
//! Callback type which is used to add callbacks to objections
//----------------------------------------------------------------------

typedef uvm_callbacks<uvm_objection, uvm_objection_callback> uvm_objection_cbs_t;

} // namespace uvm

#endif // UVM_OBJECTION_CALLBACK_H_
