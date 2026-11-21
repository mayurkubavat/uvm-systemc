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

#ifndef UVM_TR_DATABASE_H_
#define UVM_TR_DATABASE_H_

#include <fstream>
#include <string>
#include <vector>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"

namespace uvm {

class uvm_link_base;
class uvm_recorder;
class uvm_tr_stream;

//----------------------------------------------------------------------
// CLASS: uvm_tr_database
//
//! The base of a transaction database, which holds streams of
//! transactions. The database owns the streams which it opens, and
//! deletes them when it is deleted. A freed stream is kept until then, as
//! a component or a test may still refer to it; recorders, which are many
//! more, are deleted when they are freed.
//----------------------------------------------------------------------

class uvm_tr_database : public uvm_object
{
  friend class uvm_tr_stream;

 public:
  explicit uvm_tr_database( const std::string& name = "unnamed-uvm_tr_database" );
  virtual ~uvm_tr_database();

  //--------------------------------------------------------------------------
  // Group: Database API
  //--------------------------------------------------------------------------

  bool open_db();
  bool close_db();
  bool is_open() const;

  //--------------------------------------------------------------------------
  // Group: Stream API
  //--------------------------------------------------------------------------

  uvm_tr_stream* open_stream( const std::string& name,
                              const std::string& scope = "",
                              const std::string& type_name = "" );

  unsigned int get_streams( std::vector<uvm_tr_stream*>& q ) const;

  //--------------------------------------------------------------------------
  // Group: Link API
  //--------------------------------------------------------------------------

  void establish_link( uvm_link_base* link );

 protected:
  //--------------------------------------------------------------------------
  // Group: Implementation Agnostic API
  //--------------------------------------------------------------------------

  virtual bool do_open_db() = 0;
  virtual bool do_close_db() = 0;
  virtual uvm_tr_stream* do_open_stream( const std::string& name,
                                         const std::string& scope,
                                         const std::string& type_name ) = 0;
  virtual void do_establish_link( uvm_link_base* link ) = 0;

 private:
  void m_free_stream( uvm_tr_stream* stream );

  bool m_is_opened;
  std::vector<uvm_tr_stream*> m_streams; // open or closed, not freed
  std::vector<uvm_tr_stream*> m_owned;   // all the streams this database opened

}; // class uvm_tr_database

//----------------------------------------------------------------------
// CLASS: uvm_text_tr_database
//
//! The default transaction database, which writes the transactions to a
//! text file, tr_db.log unless set_file_name() names another one. The
//! file is appended to.
//----------------------------------------------------------------------

class uvm_text_tr_database : public uvm_tr_database
{
 public:
  UVM_OBJECT_UTILS(uvm_text_tr_database);

  explicit uvm_text_tr_database( const std::string& name = "unnamed-uvm_text_tr_database" );
  virtual ~uvm_text_tr_database();

  void set_file_name( const std::string& filename );

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  std::string get_file_name() const;

  //! Writes a line to the file, if the database is open
  void m_write_line( const std::string& line );

  //! Writes what is buffered to the file
  void m_flush();

 protected:
  virtual bool do_open_db();
  virtual bool do_close_db();
  virtual uvm_tr_stream* do_open_stream( const std::string& name,
                                         const std::string& scope,
                                         const std::string& type_name );
  virtual void do_establish_link( uvm_link_base* link );

 private:
  std::string m_filename;
  bool m_filename_locked;
  std::ofstream m_file;

}; // class uvm_text_tr_database

} // namespace uvm

#endif // UVM_TR_DATABASE_H_
