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

#include <algorithm>
#include <sstream>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/policy/uvm_recorder.h"
#include "uvmsc/policy/uvm_tr_database.h"
#include "uvmsc/policy/uvm_tr_stream.h"

using namespace sc_core;

namespace uvm {

//----------------------------------------------------------------------
// class uvm_tr_stream
//----------------------------------------------------------------------

std::map<int, uvm_tr_stream*>& uvm_tr_stream::m_streams_by_id()
{
  // never deleted, as the map of the recorders
  static std::map<int, uvm_tr_stream*>* streams_by_id = new std::map<int, uvm_tr_stream*>();
  return *streams_by_id;
}

uvm_tr_stream::uvm_tr_stream( const std::string& name )
: uvm_object(name),
  m_db(nullptr),
  m_configured(false),
  m_warn_null_cfg(true),
  m_is_opened(false),
  m_is_closed(false)
{}

//! Deletes the recorders of the stream
uvm_tr_stream::~uvm_tr_stream()
{
  std::vector<uvm_recorder*> records = m_records;
  m_records.clear();

  for (unsigned int i = 0; i < records.size(); i++)
  {
    records[i]->m_stream = nullptr;
    delete records[i];
  }

  m_free_handle();
}

uvm_tr_database* uvm_tr_stream::get_db() const
{
  if (!m_configured && m_warn_null_cfg)
    uvm_report_warning("UVM/REC_STR/NO_CFG", "attempt to retrieve DB from '" +
      get_name() + "' before it was set!", UVM_NONE);
  return m_db;
}

std::string uvm_tr_stream::get_scope() const
{
  return m_scope;
}

std::string uvm_tr_stream::get_stream_type_name() const
{
  return m_stream_type_name;
}

//----------------------------------------------------------------------
// member function: close
//
//! Closes the stream and its open recorders
//----------------------------------------------------------------------

void uvm_tr_stream::close()
{
  if (!is_open())
    return;

  do_close();

  std::vector<uvm_recorder*> records = m_records;
  for (unsigned int i = 0; i < records.size(); i++)
    if (records[i]->is_open())
      records[i]->close();

  m_is_opened = false;
  m_is_closed = true;
}

//----------------------------------------------------------------------
// member function: free
//
//! Frees the stream and its recorders, closing it first when it is
//! open. The recorders are deleted; the stream stays with its database,
//! which deletes it.
//----------------------------------------------------------------------

void uvm_tr_stream::free()
{
  if (!is_open() && !is_closed())
    return;

  if (is_open())
    close();

  do_free();

  std::vector<uvm_recorder*> records = m_records;
  for (unsigned int i = 0; i < records.size(); i++)
    records[i]->free();

  uvm_tr_database* db = m_db;

  m_is_closed = false;
  m_configured = false;
  m_warn_null_cfg = true;
  m_db = nullptr;
  m_free_handle();

  if (db != nullptr)
    db->m_free_stream(this);
}

void uvm_tr_stream::m_do_open( uvm_tr_database* db,
                               const std::string& scope,
                               const std::string& stream_type_name )
{
  if (db == nullptr)
  {
    uvm_report_error("UVM/REC_STR/NULL_DB", "Illegal attempt to set DB for '" +
      get_full_name() + "' to '<null>'", UVM_NONE);
    return;
  }

  if (m_configured)
  {
    uvm_report_error("UVM/REC_STR/RE_CFG", "Illegal attempt to re-open '" +
      get_full_name() + "'", UVM_NONE);
    return;
  }

  m_db = db;
  m_scope = scope;
  m_stream_type_name = stream_type_name;
  m_configured = true;
  m_is_opened = true;

  do_open(db, scope, stream_type_name);
}

bool uvm_tr_stream::is_open() const
{
  return m_is_opened;
}

bool uvm_tr_stream::is_closed() const
{
  return m_is_closed;
}

//----------------------------------------------------------------------
// member function: open_recorder
//
//! Opens a recorder in the stream; an open time of zero is the current
//! simulation time. Returns nullptr when the stream is not open.
//----------------------------------------------------------------------

uvm_recorder* uvm_tr_stream::open_recorder( const std::string& name,
                                            const sc_time& open_time,
                                            const std::string& type_name )
{
  const sc_time t = (open_time == SC_ZERO_TIME) ? sc_time_stamp() : open_time;

  if (!is_open())
    return nullptr;

  uvm_recorder* recorder = do_open_recorder(name, t, type_name);

  if (recorder != nullptr)
  {
    m_records.push_back(recorder);
    recorder->m_do_open(this, t, type_name);
  }

  return recorder;
}

void uvm_tr_stream::m_free_recorder( uvm_recorder* recorder )
{
  std::vector<uvm_recorder*>::iterator it = std::find(m_records.begin(), m_records.end(), recorder);

  if (it != m_records.end())
  {
    m_records.erase(it);
    delete recorder;
  }
}

unsigned int uvm_tr_stream::get_recorders( std::vector<uvm_recorder*>& q ) const
{
  q = m_records;
  return static_cast<unsigned int>(q.size());
}

int uvm_tr_stream::get_handle() const
{
  if (!is_open() && !is_closed())
    return 0;

  const int handle = get_inst_id();
  m_streams_by_id()[handle] = const_cast<uvm_tr_stream*>(this);
  return handle;
}

uvm_tr_stream* uvm_tr_stream::get_stream_from_handle( int id )
{
  if (id == 0)
    return nullptr;

  std::map<int, uvm_tr_stream*>::iterator it = m_streams_by_id().find(id);
  return (it == m_streams_by_id().end()) ? nullptr : it->second;
}

void uvm_tr_stream::m_free_handle()
{
  std::map<int, uvm_tr_stream*>::iterator it = m_streams_by_id().find(get_inst_id());
  if (it != m_streams_by_id().end() && it->second == this)
    m_streams_by_id().erase(it);
}

void uvm_tr_stream::do_open( uvm_tr_database*, const std::string&, const std::string& ) {}
void uvm_tr_stream::do_close() {}
void uvm_tr_stream::do_free() {}

//----------------------------------------------------------------------
// class uvm_text_tr_stream
//----------------------------------------------------------------------

uvm_text_tr_stream::uvm_text_tr_stream( const std::string& name )
: uvm_tr_stream(name), m_text_db(nullptr)
{}

void uvm_text_tr_stream::m_write( const std::string& what )
{
  if (m_text_db != nullptr && m_text_db->open_db())
  {
    std::ostringstream str;
    str << "  " << what << " @" << sc_time_stamp()
        << " {NAME:" << get_name()
        << " T:" << get_stream_type_name()
        << " SCOPE:" << get_scope()
        << " STREAM:" << get_handle() << "}";
    m_text_db->m_write_line(str.str());
  }
}

void uvm_text_tr_stream::do_open( uvm_tr_database* db,
                                  const std::string&,
                                  const std::string& )
{
  m_text_db = dynamic_cast<uvm_text_tr_database*>(db);
  m_write("CREATE_STREAM");
}

void uvm_text_tr_stream::do_close()
{
  m_write("CLOSE_STREAM");
}

void uvm_text_tr_stream::do_free()
{
  m_write("FREE_STREAM");
  m_text_db = nullptr;
}

uvm_recorder* uvm_text_tr_stream::do_open_recorder( const std::string& name,
                                                    const sc_time&,
                                                    const std::string& )
{
  if (m_text_db != nullptr && m_text_db->open_db())
    return uvm_text_recorder::type_id::create(name);

  return nullptr;
}

} // namespace uvm
