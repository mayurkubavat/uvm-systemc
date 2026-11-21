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

#include <systemc>

#include "uvmsc/base/uvm_barrier.h"
#include "uvmsc/base/uvm_object_globals.h"
#include "uvmsc/print/uvm_printer.h"

namespace uvm {

//----------------------------------------------------------------------
// Constructor
//
//! Creates a new barrier object. The \p threshold is the number of
//! processes which have to wait on the barrier before they are all
//! released.
//----------------------------------------------------------------------

uvm_barrier::uvm_barrier( const std::string& name, int threshold )
: uvm_object(name),
  m_threshold(threshold),
  m_num_waiters(0),
  m_at_threshold(false),
  m_auto_reset(true),
  m_event("barrier_" + name),
  m_generation(0)
{}

//----------------------------------------------------------------------
// member function: wait_for (virtual)
//
//! Waits for enough processes to reach the barrier before continuing.
//!
//! The number of processes to wait for is set by the member function
//! set_threshold().
//----------------------------------------------------------------------

void uvm_barrier::wait_for()
{
  if (m_at_threshold)
    return;

  m_num_waiters++;

  if (m_num_waiters >= m_threshold)
  {
    if (!m_auto_reset)
      m_at_threshold = true;

    m_trigger();
    return;
  }

  // a process which waited when the barrier was reset without wakeup
  // does not pass with those which arrive afterwards; as in UVM-SV it
  // waits forever
  const unsigned int generation = m_generation;

  do
    m_event.wait_trigger();
  while (generation != m_generation);
}

//----------------------------------------------------------------------
// member function: reset (virtual)
//
//! Resets the barrier. This sets the waiter count back to zero.
//!
//! The threshold is unchanged. After reset, the barrier will force
//! processes to wait for the threshold again.
//!
//! If the \p wakeup bit is set, any currently waiting processes will
//! be activated.
//! Otherwise they are left waiting, and are not counted any more.
//----------------------------------------------------------------------

void uvm_barrier::reset( bool wakeup )
{
  m_at_threshold = false;

  if (m_num_waiters)
  {
    if (wakeup)
      m_event.trigger();
    else
    {
      m_generation++;
      m_event.reset();
    }
  }

  m_num_waiters = 0;
}

//----------------------------------------------------------------------
// member function: set_auto_reset (virtual)
//
//! Determines if the barrier should reset itself after the threshold
//! is reached.
//!
//! The default is on, so when a barrier hits its threshold it will
//! reset, and new processes will block until the threshold is reached
//! again.
//!
//! If auto reset is off, then once the threshold is achieved, new
//! processes pass through without being blocked until the barrier is
//! reset.
//----------------------------------------------------------------------

void uvm_barrier::set_auto_reset( bool value )
{
  m_at_threshold = false;
  m_auto_reset = value;
}

//----------------------------------------------------------------------
// member function: set_threshold (virtual)
//
//! Sets the process threshold.
//!
//! This determines how many processes must be waiting on the barrier
//! before the processes may proceed.
//!
//! Once the \p threshold is reached, all waiting processes are
//! activated.
//!
//! If \p threshold is set to a value less than the number of currently
//! waiting processes, then the barrier is reset and waiting processes
//! are activated.
//----------------------------------------------------------------------

void uvm_barrier::set_threshold( int threshold )
{
  m_threshold = threshold;

  if (threshold <= m_num_waiters)
    reset(true);
}

//----------------------------------------------------------------------
// member function: get_threshold (virtual)
//
//! Gets the current threshold setting for the barrier.
//----------------------------------------------------------------------

int uvm_barrier::get_threshold() const
{
  return m_threshold;
}

//----------------------------------------------------------------------
// member function: get_num_waiters (virtual)
//
//! Returns the number of processes currently waiting at the barrier.
//----------------------------------------------------------------------

int uvm_barrier::get_num_waiters() const
{
  return m_num_waiters;
}

//----------------------------------------------------------------------
// member function: cancel (virtual)
//
//! Decrements the waiter count by one. This is used when a process
//! that is waiting on the barrier is killed or activated by some other
//! means.
//----------------------------------------------------------------------

void uvm_barrier::cancel()
{
  m_event.cancel();

  // not the count of the event: that may include processes which were
  // left behind by a reset without wakeup
  if (m_num_waiters > 0)
    m_num_waiters--;
}

//----------------------------------------------------------------------
// member function: m_trigger
//
// Implementation defined
//----------------------------------------------------------------------

void uvm_barrier::m_trigger()
{
  m_event.trigger();
  m_num_waiters = 0;

  // this process was last to wait; allow other processes to resume first
  sc_core::wait(sc_core::SC_ZERO_TIME);
}

//----------------------------------------------------------------------
// member function: do_print (virtual)
//
// Implementation defined
//----------------------------------------------------------------------

void uvm_barrier::do_print( const uvm_printer& printer ) const
{
  printer.print_field_int("threshold", m_threshold, 32, UVM_DEC, ".", "int");
  printer.print_field_int("num_waiters", m_num_waiters, 32, UVM_DEC, ".", "int");
  printer.print_field_int("at_threshold", m_at_threshold, 1, UVM_BIN, ".", "bit");
  printer.print_field_int("auto_reset", m_auto_reset, 1, UVM_BIN, ".", "bit");
}

//----------------------------------------------------------------------
// member function: do_copy (virtual)
//
// Implementation defined
//----------------------------------------------------------------------

void uvm_barrier::do_copy( const uvm_object& rhs )
{
  const uvm_barrier* b = dynamic_cast<const uvm_barrier*>(&rhs);

  uvm_object::do_copy(rhs);

  if (b == nullptr)
    return;

  m_threshold = b->m_threshold;
  m_at_threshold = b->m_at_threshold;
  m_auto_reset = b->m_auto_reset;

  // In UVM-SV the copy shares the event, and with it the waiting
  // processes, of the barrier it was copied from. Here every barrier has
  // its own event, so the copy starts without waiting processes.
  m_num_waiters = 0;
}

} // namespace uvm
