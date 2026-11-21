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

#ifndef UVM_TR_STREAM_H_
#define UVM_TR_STREAM_H_

#include <map>
#include <string>
#include <vector>

#include <systemc>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"

namespace uvm {

class uvm_recorder;
class uvm_tr_database;
class uvm_text_tr_database;

//----------------------------------------------------------------------
// CLASS: uvm_tr_stream
//
//! A stream of a transaction database, which holds the recorders of its
//! transactions. A stream is created by uvm_tr_database::open_stream()
//! and belongs to its database, which deletes it. The stream owns its
//! recorders.
//----------------------------------------------------------------------

class uvm_tr_stream : public uvm_object
{
  friend class uvm_tr_database;
  friend class uvm_recorder;

 public:
  explicit uvm_tr_stream( const std::string& name = "unnamed-uvm_tr_stream" );
  virtual ~uvm_tr_stream();

  //--------------------------------------------------------------------------
  // Group: Configuration API
  //--------------------------------------------------------------------------

  uvm_tr_database* get_db() const;
  std::string get_scope() const;
  std::string get_stream_type_name() const;

  //--------------------------------------------------------------------------
  // Group: Stream API
  //--------------------------------------------------------------------------

  void close();
  void free();

  bool is_open() const;
  bool is_closed() const;

  //--------------------------------------------------------------------------
  // Group: Transaction Recorder API
  //--------------------------------------------------------------------------

  uvm_recorder* open_recorder( const std::string& name,
                               const sc_core::sc_time& open_time = sc_core::SC_ZERO_TIME,
                               const std::string& type_name = "" );

  unsigned int get_recorders( std::vector<uvm_recorder*>& q ) const;

  //--------------------------------------------------------------------------
  // Group: Handles
  //--------------------------------------------------------------------------

  int get_handle() const;
  static uvm_tr_stream* get_stream_from_handle( int id );

 protected:
  //--------------------------------------------------------------------------
  // Group: Implementation Agnostic API
  //--------------------------------------------------------------------------

  virtual void do_open( uvm_tr_database* db,
                        const std::string& scope,
                        const std::string& stream_type_name );
  virtual void do_close();
  virtual void do_free();

  virtual uvm_recorder* do_open_recorder( const std::string& name,
                                          const sc_core::sc_time& open_time,
                                          const std::string& type_name ) = 0;

 private:
  void m_do_open( uvm_tr_database* db,
                  const std::string& scope,
                  const std::string& stream_type_name );
  void m_free_recorder( uvm_recorder* recorder );
  void m_free_handle();

  static std::map<int, uvm_tr_stream*>& m_streams_by_id();

  uvm_tr_database* m_db;
  std::string m_scope;
  std::string m_stream_type_name;
  bool m_configured;
  bool m_warn_null_cfg;
  bool m_is_opened;
  bool m_is_closed;
  std::vector<uvm_recorder*> m_records;

}; // class uvm_tr_stream

//----------------------------------------------------------------------
// CLASS: uvm_text_tr_stream
//
//! The stream of the uvm_text_tr_database
//----------------------------------------------------------------------

class uvm_text_tr_stream : public uvm_tr_stream
{
 public:
  UVM_OBJECT_UTILS(uvm_text_tr_stream);

  explicit uvm_text_tr_stream( const std::string& name = "unnamed-uvm_text_tr_stream" );

 protected:
  virtual void do_open( uvm_tr_database* db,
                        const std::string& scope,
                        const std::string& stream_type_name );
  virtual void do_close();
  virtual void do_free();

  virtual uvm_recorder* do_open_recorder( const std::string& name,
                                          const sc_core::sc_time& open_time,
                                          const std::string& type_name );

 private:
  void m_write( const std::string& what );

  uvm_text_tr_database* m_text_db;

}; // class uvm_text_tr_stream

} // namespace uvm

#endif // UVM_TR_STREAM_H_
