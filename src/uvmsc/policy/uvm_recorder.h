//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2007-2014 Mentor Graphics Corporation
//   Copyright 2013-2024 NVIDIA Corporation
//   Copyright 2013 NXP B.V.
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

#ifndef UVM_RECORDER_H_
#define UVM_RECORDER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <systemc>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_object_globals.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"

namespace uvm {

class uvm_tr_stream;
class uvm_text_tr_database;

//----------------------------------------------------------------------
// CLASS: uvm_recorder
//
//! Records the attributes of one transaction into the stream of a
//! transaction database. A recorder is created by
//! uvm_tr_stream::open_recorder() and belongs to its stream: it is
//! deleted when it is freed, so it shall not be used after free(); use
//! its handle to find it again (get_recorder_from_handle()).
//!
//! This base class keeps the state of the recorder and does not store
//! anything: an extension implements the do_* member functions, as
//! uvm_text_recorder does.
//----------------------------------------------------------------------

class uvm_recorder : public uvm_object
{
  friend class uvm_tr_stream;

 public:
  UVM_OBJECT_UTILS(uvm_recorder);

  explicit uvm_recorder( const std::string& name = "uvm_recorder" );
  virtual ~uvm_recorder();

  //--------------------------------------------------------------------------
  // Group: Configuration API
  //--------------------------------------------------------------------------

  //! The default radix of record_field() without a radix
  uvm_radix_enum default_radix;

  //! Filters of the fields which do_record() may test
  bool physical;
  bool abstract;

  //! Records the instance ids of the objects
  bool identifier;

  //! The recursion policy for recording objects
  uvm_recursion_policy_enum policy;

  virtual void set_recursion_policy( uvm_recursion_policy_enum p );
  virtual uvm_recursion_policy_enum get_recursion_policy() const;

  virtual void set_id_enabled( bool enabled );
  virtual bool get_id_enabled() const;

  virtual void set_default_radix( uvm_radix_enum radix );
  virtual uvm_radix_enum get_default_radix() const;

  //--------------------------------------------------------------------------
  // Group: Transaction Recorder API
  //--------------------------------------------------------------------------

  uvm_tr_stream* get_stream() const;

  void close( const sc_core::sc_time& close_time = sc_core::SC_ZERO_TIME );
  void free( const sc_core::sc_time& close_time = sc_core::SC_ZERO_TIME );

  bool is_open() const;
  sc_core::sc_time get_open_time() const;

  bool is_closed() const;
  sc_core::sc_time get_close_time() const;

  //--------------------------------------------------------------------------
  // Group: Handles
  //--------------------------------------------------------------------------

  int get_handle() const;
  static uvm_recorder* get_recorder_from_handle( int id );

  //--------------------------------------------------------------------------
  // Group: Attribute Recording
  //--------------------------------------------------------------------------

  void record_field( const std::string& name,
                     const uvm_bitstream_t& value,
                     int size,
                     uvm_radix_enum radix = UVM_NORADIX );

  void record_field_int( const std::string& name,
                         const uvm_integral_t& value,
                         int size,
                         uvm_radix_enum radix = UVM_NORADIX );

  void record_field_real( const std::string& name, double value );

  void record_object( const std::string& name, uvm_object* value );

  void record_string( const std::string& name, const std::string& value );

  void record_time( const std::string& name, const sc_core::sc_time& value );

  void record_generic( const std::string& name,
                       const std::string& value,
                       const std::string& type_name = "" );

  virtual bool use_record_attribute() const;
  virtual int get_record_attribute_handle() const;

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  //! The number of objects which are being recorded, one inside the other
  int get_active_object_depth() const;

  //! Kept for compatibility: the depth of the recording
  int recording_depth;

 protected:
  //--------------------------------------------------------------------------
  // Group: Implementation Agnostic API
  //--------------------------------------------------------------------------

  virtual void do_open( uvm_tr_stream* stream,
                        const sc_core::sc_time& open_time,
                        const std::string& type_name );
  virtual void do_close( const sc_core::sc_time& close_time );
  virtual void do_free();

  virtual void do_record_field( const std::string& name,
                                const uvm_bitstream_t& value,
                                int size,
                                uvm_radix_enum radix );

  virtual void do_record_field_int( const std::string& name,
                                    const uvm_integral_t& value,
                                    int size,
                                    uvm_radix_enum radix );

  virtual void do_record_field_real( const std::string& name, double value );

  virtual void do_record_object( const std::string& name, uvm_object* value );

  virtual void do_record_string( const std::string& name, const std::string& value );

  virtual void do_record_time( const std::string& name, const sc_core::sc_time& value );

  virtual void do_record_generic( const std::string& name,
                                  const std::string& value,
                                  const std::string& type_name );

 private:
  void m_do_open( uvm_tr_stream* stream,
                  const sc_core::sc_time& open_time,
                  const std::string& type_name );
  void m_free_handle();

  static std::map<int, uvm_recorder*>& m_recorders_by_id();

  uvm_tr_stream* m_stream;
  bool m_is_opened;
  bool m_is_closed;
  sc_core::sc_time m_open_time;
  sc_core::sc_time m_close_time;
  std::vector<uvm_object*> m_active_objects;

}; // class uvm_recorder

//----------------------------------------------------------------------
// CLASS: uvm_text_recorder
//
//! The recorder of the uvm_text_tr_database, which writes the
//! attributes to the text file of the database
//----------------------------------------------------------------------

class uvm_text_recorder : public uvm_recorder
{
 public:
  UVM_OBJECT_UTILS(uvm_text_recorder);

  explicit uvm_text_recorder( const std::string& name = "unnamed-uvm_text_recorder" );

  void write_attribute( const std::string& nm,
                        const uvm_bitstream_t& value,
                        uvm_radix_enum radix,
                        int numbits = UVM_MAX_STREAMBITS );

  void write_attribute_int( const std::string& nm,
                            const uvm_integral_t& value,
                            uvm_radix_enum radix,
                            int numbits = 64 );

 protected:
  virtual void do_open( uvm_tr_stream* stream,
                        const sc_core::sc_time& open_time,
                        const std::string& type_name );
  virtual void do_close( const sc_core::sc_time& close_time );
  virtual void do_free();

  virtual void do_record_field( const std::string& name,
                                const uvm_bitstream_t& value,
                                int size,
                                uvm_radix_enum radix );

  virtual void do_record_field_int( const std::string& name,
                                    const uvm_integral_t& value,
                                    int size,
                                    uvm_radix_enum radix );

  virtual void do_record_field_real( const std::string& name, double value );

  virtual void do_record_object( const std::string& name, uvm_object* value );

  virtual void do_record_string( const std::string& name, const std::string& value );

  virtual void do_record_time( const std::string& name, const sc_core::sc_time& value );

  virtual void do_record_generic( const std::string& name,
                                  const std::string& value,
                                  const std::string& type_name );

 private:
  std::string m_current_context( const std::string& name = "" ) const;
  void m_write_attr( const std::string& nm, const std::string& value,
                     const std::string& radix, int numbits );

  uvm_text_tr_database* m_text_db;
  std::vector<std::string> m_object_names;

}; // class uvm_text_recorder

} // namespace uvm

#endif // UVM_RECORDER_H_
