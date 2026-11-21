//----------------------------------------------------------------------
//   Copyright 2012-2020 NXP B.V.
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

#ifndef UVM_TRANSACTION_H_
#define UVM_TRANSACTION_H_

#include <systemc>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_event.h"

namespace uvm {

// forward class declarations
class uvm_printer;
class uvm_component;
class uvm_recorder;
class uvm_tr_stream;

//------------------------------------------------------------------------------
// CLASS: uvm_transaction
//
//! The uvm_transaction class is the root base class for UVM transactions.
//! Inheriting all the methods of uvm_object, uvm_transaction adds a timing and
//! recording interface.
//!
//! This class provides timestamp properties, notification events, and transaction
//! recording support.
//!
//! Derived from uvm_object.
//------------------------------------------------------------------------------

class uvm_transaction : public uvm_object
{
 public:
  friend class uvm_reg_map;
  friend class uvm_sequence_base;
  friend class uvm_sequence_item;

  uvm_transaction();

  explicit uvm_transaction( uvm_object_name name );

  // A copy has the fields and the stream of the transaction, but does not
  // take part in its recording
  uvm_transaction( const uvm_transaction& rhs );
  uvm_transaction& operator=( const uvm_transaction& rhs );

  void set_transaction_id(int id);

  int get_transaction_id() const;

  // Transaction recording

  void accept_tr( const sc_core::sc_time& accept_time = sc_core::SC_ZERO_TIME );

  int begin_tr( const sc_core::sc_time& begin_time = sc_core::SC_ZERO_TIME,
                int parent_handle = 0 );

  int begin_child_tr( const sc_core::sc_time& begin_time = sc_core::SC_ZERO_TIME,
                      int parent_handle = 0 );

  void end_tr( const sc_core::sc_time& end_time = sc_core::SC_ZERO_TIME,
               bool free_handle = true );

  int get_tr_handle() const;

  void enable_recording( uvm_tr_stream* stream );
  void disable_recording();
  bool is_recording_enabled() const;

  bool is_active() const;

  sc_core::sc_time get_accept_time() const;
  sc_core::sc_time get_begin_time() const;
  sc_core::sc_time get_end_time() const;

  void set_initiator( uvm_component* initiator );
  uvm_component* get_initiator() const;

  virtual void do_record( const uvm_recorder& recorder );


  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

protected:

  virtual ~uvm_transaction();

  virtual void do_print( const uvm_printer& printer ) const;

  virtual void do_accept_tr();
  virtual void do_begin_tr();
  virtual void do_end_tr();

private:

  void m_init_transaction();
  void m_reset_tr_state();

  // member variables
  int m_transaction_id; /*!< The transaction id data member. */
  uvm_component* initiator;

  sc_core::sc_time begin_time;
  sc_core::sc_time end_time;
  sc_core::sc_time accept_time;

  uvm_event end_event;

  int m_begin_tr( const sc_core::sc_time& begin_time, int parent_handle );

  uvm_tr_stream* m_stream;  // recording is enabled on this stream
  int m_recorder_handle;    // the handle of the recorder of the current transaction
  uvm_recorder* m_get_recorder() const;
  bool m_active;

};

} /* namespace uvm */

#endif /* UVM_TRANSACTION_H_ */
