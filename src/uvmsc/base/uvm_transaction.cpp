//----------------------------------------------------------------------
//   Copyright 2014 Fraunhofer-Gesellschaft zur Foerderung
//					der angewandten Forschung e.V.
//   Copyright 2012-2015 NXP B.V.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2011 Cadence Design Systems, Inc.
//   Copyright 2010 Synopsys, Inc.
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

#include <sstream>

#include "uvmsc/base/uvm_transaction.h"
#include "uvmsc/print/uvm_printer.h"
#include "uvmsc/base/uvm_component.h"
#include "uvmsc/policy/uvm_links.h"
#include "uvmsc/policy/uvm_recorder.h"
#include "uvmsc/policy/uvm_tr_database.h"
#include "uvmsc/policy/uvm_tr_stream.h"

using namespace sc_core;

namespace uvm {

//----------------------------------------------------------------------
// Constuctors
//----------------------------------------------------------------------

uvm_transaction::uvm_transaction() : uvm_object( sc_core::sc_gen_unique_name( "transaction" ))
{
  m_init_transaction();
}

uvm_transaction::uvm_transaction( uvm_object_name name ) : uvm_object( name )
{
  m_init_transaction();
}

//----------------------------------------------------------------------
// member function: set_transaction_id
//
//! Sets this transactions numeric identifier to id. If not set via this
//! method, the transaction ID defaults to -1.
//! When using sequences to generate stimulus, the transaction ID is used
//! along with the sequence ID to route responses in sequencers and to
//! correlate responses to requests.
//----------------------------------------------------------------------

void uvm_transaction::set_transaction_id(int id)
{
  m_transaction_id = id;
}

//----------------------------------------------------------------------
// member function:  get_transaction_id
//
//! Returns this transactions numeric identifier, which is -1 if not set
//! explicitly by set_transaction_id.
//! When using a uvm_sequence<REQ,RSP> to generate stimulus, the transaction
//! ID is used along with the sequence ID to route responses in sequencers
//! and to correlate responses to requests.
//----------------------------------------------------------------------

int uvm_transaction::get_transaction_id() const
{
  return m_transaction_id;
}

//----------------------------------------------------------------------
// member function: accept_tr
//
//! Marks the time at which the transaction is accepted. An accept time
//! of zero is the current simulation time.
//----------------------------------------------------------------------

void uvm_transaction::accept_tr( const sc_time& accept_time )
{
  this->accept_time = (accept_time == SC_ZERO_TIME) ? sc_time_stamp() : accept_time;
  do_accept_tr();
}

//----------------------------------------------------------------------
// member function: begin_tr
//
//! Starts the transaction. When recording is enabled, it opens a
//! recorder on the stream of the transaction, linked as a child to the
//! recorder of \p parent_handle if one is given. Returns the handle of
//! the recorder, or 0 when the transaction is not recorded.
//----------------------------------------------------------------------

int uvm_transaction::begin_tr( const sc_time& begin_time, int parent_handle )
{
  return m_begin_tr(begin_time, parent_handle);
}

int uvm_transaction::begin_child_tr( const sc_time& begin_time, int parent_handle )
{
  return m_begin_tr(begin_time, parent_handle);
}

int uvm_transaction::m_begin_tr( const sc_time& begin_time, int parent_handle )
{
  const sc_time tmp_time = (begin_time == SC_ZERO_TIME) ? sc_time_stamp() : begin_time;
  uvm_recorder* parent_recorder = nullptr;
  int handle = 0;

  if (parent_handle != 0)
    parent_recorder = uvm_recorder::get_recorder_from_handle(parent_handle);

  // a transaction which is still being recorded ends first
  if (m_get_recorder() != nullptr)
    end_tr(tmp_time);

  m_active = true;
  this->begin_time = tmp_time;

  if (is_recording_enabled())
  {
    uvm_tr_database* db = m_stream->get_db();

    uvm_recorder* recorder = m_stream->open_recorder(get_type_name(), this->begin_time,
      (parent_recorder == nullptr) ? "Begin_No_Parent, Link" : "Begin_End, Link");

    if (recorder != nullptr)
    {
      if (parent_recorder != nullptr && db != nullptr)
      {
        uvm_parent_child_link link("pc_link");
        link.set(parent_recorder, recorder);
        db->establish_link(&link);
      }

      handle = recorder->get_handle();
    }
  }

  m_recorder_handle = handle;

  do_begin_tr();

  return handle;
}

//----------------------------------------------------------------------
// member function: end_tr
//
//! Ends the transaction. When it is recorded, its fields are recorded,
//! and the recorder is closed, and freed unless \p free_handle is false.
//! The end event of the transaction is triggered.
//----------------------------------------------------------------------

void uvm_transaction::end_tr( const sc_time& end_time, bool free_handle )
{
  this->end_time = (end_time == SC_ZERO_TIME) ? sc_time_stamp() : end_time;
  m_active = false;

  do_end_tr();

  // the recorder is found by its handle: it is gone when its stream was
  // freed in the meantime
  uvm_recorder* recorder = m_get_recorder();
  m_recorder_handle = 0;

  if (is_recording_enabled() && recorder != nullptr)
  {
    record(recorder);
    recorder->close(this->end_time);

    if (free_handle)
      recorder->free();
  }

  end_event.trigger();
}

int uvm_transaction::get_tr_handle() const
{
  return (m_get_recorder() != nullptr) ? m_recorder_handle : 0;
}

uvm_recorder* uvm_transaction::m_get_recorder() const
{
  return uvm_recorder::get_recorder_from_handle(m_recorder_handle);
}

void uvm_transaction::enable_recording( uvm_tr_stream* stream )
{
  m_stream = stream;
}

void uvm_transaction::disable_recording()
{
  m_stream = nullptr;
}

bool uvm_transaction::is_recording_enabled() const
{
  return m_stream != nullptr;
}

bool uvm_transaction::is_active() const
{
  return m_active;
}

sc_time uvm_transaction::get_accept_time() const
{
  return accept_time;
}

sc_time uvm_transaction::get_begin_time() const
{
  return begin_time;
}

sc_time uvm_transaction::get_end_time() const
{
  return end_time;
}

void uvm_transaction::set_initiator( uvm_component* initiator )
{
  this->initiator = initiator;
}

uvm_component* uvm_transaction::get_initiator() const
{
  return initiator;
}

void uvm_transaction::do_accept_tr() {}
void uvm_transaction::do_begin_tr() {}
void uvm_transaction::do_end_tr() {}

//----------------------------------------------------------------------
// member function: do_record (virtual)
//
//! Records the accept time and the initiator of the transaction. Other
//! than in UVM-SV, the time is recorded with record_time() and the
//! initiator by its full name; an accept time of zero means that the
//! transaction was not accepted.
//----------------------------------------------------------------------

void uvm_transaction::do_record( const uvm_recorder& recorder )
{
  uvm_object::do_record(recorder);

  uvm_recorder* r = const_cast<uvm_recorder*>(&recorder);

  if (accept_time != SC_ZERO_TIME)
    r->record_time("accept_time", accept_time);

  if (initiator != nullptr)
    r->record_string("initiator", initiator->get_full_name());
}


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//////// Implementation-defined member functions start here ////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------
// Copy constructor and assignment
//
// Implementation defined
// A copy gets the fields and the stream of the transaction, but not its
// recorder: only the transaction which began recording ends it.
//----------------------------------------------------------------------

uvm_transaction::uvm_transaction( const uvm_transaction& rhs )
: uvm_object(rhs),
  m_transaction_id(rhs.m_transaction_id),
  initiator(rhs.initiator),
  begin_time(rhs.begin_time),
  end_time(rhs.end_time),
  accept_time(rhs.accept_time),
  end_event(rhs.end_event),
  m_stream(rhs.m_stream),
  m_recorder_handle(0),
  m_active(false)
{}

uvm_transaction& uvm_transaction::operator=( const uvm_transaction& rhs )
{
  if (this != &rhs)
  {
    uvm_object::operator=(rhs);
    m_transaction_id = rhs.m_transaction_id;
    initiator = rhs.initiator;
    begin_time = rhs.begin_time;
    end_time = rhs.end_time;
    accept_time = rhs.accept_time;
    end_event = rhs.end_event;
    m_stream = rhs.m_stream;
    // the recorder of this transaction stays with it
  }
  return *this;
}

//----------------------------------------------------------------------
// member function: m_init_transaction
//
// Implementation defined
// Helper functions to initialize transaction object
//----------------------------------------------------------------------

void uvm_transaction::m_init_transaction()
{
  m_transaction_id =-1;
  initiator = nullptr; // TODO assign parent component

  begin_time = SC_ZERO_TIME; // TODO replace by sc_max_time?
  end_time = SC_ZERO_TIME;
  accept_time = SC_ZERO_TIME;
  m_stream = nullptr;
  m_recorder_handle = 0;
  m_active = false;
}


//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------

uvm_transaction::~uvm_transaction()
{
  // TODO cleanup
  //std::cout << "destructor of uvm_transaction " << get_full_name() << std::endl;
}

//----------------------------------------------------------------------
// member function: do_print
//
//! Implementation-defined member function
//----------------------------------------------------------------------

void uvm_transaction::do_print( const uvm_printer& printer ) const
{
  std::string str;

  uvm_object::do_print(printer);

  // TODO replace by sc_max_time?
  if(accept_time != SC_ZERO_TIME )
    printer.print_time("accept_time", accept_time);

  if(begin_time != SC_ZERO_TIME )
    printer.print_time("begin_time", begin_time);

  if(end_time != SC_ZERO_TIME )
    printer.print_time("end_time", end_time);

  if(initiator != nullptr)
  {
    std::ostringstream str;
    str << "@" << initiator->get_inst_id();
    printer.print_generic("initiator", initiator->get_type_name(), -1, str.str());
  }
}

} /* namespace uvm */
