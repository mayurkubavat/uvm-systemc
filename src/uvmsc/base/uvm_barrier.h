//----------------------------------------------------------------------
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2010 Cadence Design Systems, Inc.
//   Copyright 2010 Synopsys, Inc.
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

#ifndef UVM_BARRIER_H_
#define UVM_BARRIER_H_

#include <string>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_event.h"
#include "uvmsc/conf/uvm_object_string_pool.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"

namespace uvm {

// forward class declarations
class uvm_printer;

//----------------------------------------------------------------------
// CLASS: uvm_barrier
//
//! The uvm_barrier class provides a multiprocess synchronization
//! mechanism. It enables a set of processes to block until the desired
//! number of processes get to the synchronization point, at which time
//! all of the processes are released.
//----------------------------------------------------------------------

class uvm_barrier : public uvm_object
{
 public:
  UVM_OBJECT_UTILS(uvm_barrier);

  //--------------------------------------------------------------------
  // UVM Standard LRM API below
  //--------------------------------------------------------------------

  explicit uvm_barrier( const std::string& name = "", int threshold = 0 );

  virtual void wait_for();

  virtual void reset( bool wakeup = true );

  virtual void set_auto_reset( bool value = true );

  virtual void set_threshold( int threshold );

  virtual int get_threshold() const;

  virtual int get_num_waiters() const;

  virtual void cancel();

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  virtual void do_print( const uvm_printer& printer ) const;

  virtual void do_copy( const uvm_object& rhs );

 private:
  void m_trigger();

  int       m_threshold;
  int       m_num_waiters;
  bool      m_at_threshold;
  bool      m_auto_reset;
  uvm_event m_event;
  unsigned int m_generation; // counts the resets without wakeup

}; // class uvm_barrier

//----------------------------------------------------------------------
// Typedef: uvm_barrier_pool
//
//! A pool of barriers, indexed by name
//----------------------------------------------------------------------

typedef uvm_object_string_pool<uvm_barrier> uvm_barrier_pool;

} // namespace uvm

#endif // UVM_BARRIER_H_
