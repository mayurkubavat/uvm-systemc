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
#include <cstdlib>
#include <set>
#include <sstream>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/policy/uvm_links.h"
#include "uvmsc/policy/uvm_recorder.h"
#include "uvmsc/policy/uvm_tr_database.h"
#include "uvmsc/policy/uvm_tr_stream.h"

using namespace sc_core;

namespace uvm {

//----------------------------------------------------------------------
// class uvm_tr_database
//----------------------------------------------------------------------

uvm_tr_database::uvm_tr_database( const std::string& name )
: uvm_object(name), m_is_opened(false)
{}

//! Deletes the streams which the database opened
uvm_tr_database::~uvm_tr_database()
{
  std::vector<uvm_tr_stream*> owned = m_owned;
  m_owned.clear();
  m_streams.clear();

  for (unsigned int i = 0; i < owned.size(); i++)
  {
    owned[i]->m_db = nullptr;
    delete owned[i];
  }
}

bool uvm_tr_database::open_db()
{
  if (!m_is_opened)
    m_is_opened = do_open_db();
  return m_is_opened;
}

bool uvm_tr_database::close_db()
{
  if (m_is_opened && do_close_db())
    m_is_opened = false;
  return !m_is_opened;
}

bool uvm_tr_database::is_open() const
{
  return m_is_opened;
}

//----------------------------------------------------------------------
// member function: open_stream
//
//! Opens a stream in the database, opening the database first when
//! needed. Returns nullptr when the database cannot be opened.
//----------------------------------------------------------------------

uvm_tr_stream* uvm_tr_database::open_stream( const std::string& name,
                                             const std::string& scope,
                                             const std::string& type_name )
{
  if (!open_db())
    return nullptr;

  uvm_tr_stream* stream = do_open_stream(name, scope, type_name);

  if (stream != nullptr)
  {
    m_streams.push_back(stream);
    if (std::find(m_owned.begin(), m_owned.end(), stream) == m_owned.end())
      m_owned.push_back(stream);
    stream->m_do_open(this, scope, type_name);
  }

  return stream;
}

void uvm_tr_database::m_free_stream( uvm_tr_stream* stream )
{
  std::vector<uvm_tr_stream*>::iterator it = std::find(m_streams.begin(), m_streams.end(), stream);
  if (it != m_streams.end())
    m_streams.erase(it);
}

unsigned int uvm_tr_database::get_streams( std::vector<uvm_tr_stream*>& q ) const
{
  q = m_streams;
  return static_cast<unsigned int>(q.size());
}

//----------------------------------------------------------------------
// member function: establish_link
//
//! Establishes a link between two streams or recorders of this database
//----------------------------------------------------------------------

void uvm_tr_database::establish_link( uvm_link_base* link )
{
  if (link == nullptr)
    return;

  uvm_object* lhs = link->get_lhs();
  uvm_object* rhs = link->get_rhs();

  if (lhs == nullptr)
  {
    uvm_report_warning("UVM/TR_DB/BAD_LINK",
      "left hand side '<null>' is not supported in links for 'uvm_tr_database'", UVM_NONE);
    return;
  }

  if (rhs == nullptr)
  {
    uvm_report_warning("UVM/TR_DB/BAD_LINK",
      "right hand side '<null>' is not supported in links for 'uvm_tr_database'", UVM_NONE);
    return;
  }

  uvm_tr_stream* s_lhs = dynamic_cast<uvm_tr_stream*>(lhs);
  uvm_tr_stream* s_rhs = dynamic_cast<uvm_tr_stream*>(rhs);
  uvm_recorder* r_lhs = dynamic_cast<uvm_recorder*>(lhs);
  uvm_recorder* r_rhs = dynamic_cast<uvm_recorder*>(rhs);

  if (s_lhs == nullptr && r_lhs == nullptr)
  {
    uvm_report_warning("UVM/TR_DB/BAD_LINK", "left hand side of type '" + lhs->get_type_name() +
      "' not supported in links for 'uvm_tr_database'", UVM_NONE);
    return;
  }

  if (s_rhs == nullptr && r_rhs == nullptr)
  {
    uvm_report_warning("UVM/TR_DB/BAD_LINK", "right hand side of type '" + rhs->get_type_name() +
      "' not supported in links for 'uvm_tr_database'", UVM_NONE);
    return;
  }

  if (r_lhs != nullptr)
    s_lhs = r_lhs->get_stream();

  if (r_rhs != nullptr)
    s_rhs = r_rhs->get_stream();

  if (s_lhs != nullptr && s_lhs->get_db() != this)
  {
    uvm_report_warning("UVM/TR_DB/BAD_LINK", "attempt to link stream from '" +
      ((s_lhs->get_db() != nullptr) ? s_lhs->get_db()->get_name() : std::string("<null>")) +
      "' into '" + get_name() + "'", UVM_NONE);
    return;
  }

  if (s_rhs != nullptr && s_rhs->get_db() != this)
  {
    uvm_report_warning("UVM/TR_DB/BAD_LINK", "attempt to link stream from '" +
      ((s_rhs->get_db() != nullptr) ? s_rhs->get_db()->get_name() : std::string("<null>")) +
      "' into '" + get_name() + "'", UVM_NONE);
    return;
  }

  do_establish_link(link);
}

//----------------------------------------------------------------------
// class uvm_text_tr_database
//
// The file is written with a buffer, and flushed when the database is
// closed or deleted, and when the program exits, also for a database
// which is never deleted, such as the default one.
//----------------------------------------------------------------------

namespace {

std::set<uvm_text_tr_database*>& m_text_databases()
{
  static std::set<uvm_text_tr_database*>* databases = new std::set<uvm_text_tr_database*>();
  return *databases;
}

void m_flush_text_databases()
{
  std::set<uvm_text_tr_database*>& databases = m_text_databases();
  for (std::set<uvm_text_tr_database*>::iterator it = databases.begin(); it != databases.end(); it++)
    (*it)->m_flush();
}

} // namespace

uvm_text_tr_database::uvm_text_tr_database( const std::string& name )
: uvm_tr_database(name), m_filename("tr_db.log"), m_filename_locked(false)
{
  static bool flush_at_exit = (std::atexit(&m_flush_text_databases) == 0);
  (void)flush_at_exit;
  m_text_databases().insert(this);
}

uvm_text_tr_database::~uvm_text_tr_database()
{
  m_text_databases().erase(this);

  if (m_file.is_open())
    m_file.close();
}

void uvm_text_tr_database::m_flush()
{
  if (m_file.is_open())
    m_file.flush();
}

bool uvm_text_tr_database::do_open_db()
{
  if (!m_file.is_open())
  {
    m_file.open(m_filename.c_str(), std::ios::out | std::ios::app);
    if (m_file.is_open())
      m_filename_locked = true;
  }
  return m_file.is_open();
}

bool uvm_text_tr_database::do_close_db()
{
  if (m_file.is_open())
  {
    m_file.close();
    m_filename_locked = false;
  }
  return true;
}

uvm_tr_stream* uvm_text_tr_database::do_open_stream( const std::string& name,
                                                     const std::string&,
                                                     const std::string& )
{
  return uvm_text_tr_stream::type_id::create(name);
}

void uvm_text_tr_database::do_establish_link( uvm_link_base* link )
{
  uvm_recorder* r_lhs = dynamic_cast<uvm_recorder*>(link->get_lhs());
  uvm_recorder* r_rhs = dynamic_cast<uvm_recorder*>(link->get_rhs());

  if (r_lhs == nullptr || r_rhs == nullptr)
    return;

  std::string relation;

  if (dynamic_cast<uvm_parent_child_link*>(link) != nullptr)
    relation = "child";
  else if (dynamic_cast<uvm_related_link*>(link) == nullptr)
    return;

  std::ostringstream str;
  str << "  LINK @" << sc_time_stamp()
      << " {TXH1:" << r_lhs->get_handle()
      << " TXH2:" << r_rhs->get_handle()
      << " RELATION=" << relation << "}";
  m_write_line(str.str());
}

void uvm_text_tr_database::set_file_name( const std::string& filename )
{
  if (filename.empty())
  {
    uvm_report_warning("UVM/TXT_DB/EMPTY_NAME", "Ignoring attempt to set file name to ''!", UVM_NONE);
    return;
  }

  if (m_filename_locked)
  {
    uvm_report_warning("UVM/TXT_DB/SET_AFTER_OPEN",
      "Ignoring attempt to change file name after opening the db!", UVM_NONE);
    return;
  }

  m_filename = filename;
}

std::string uvm_text_tr_database::get_file_name() const
{
  return m_filename;
}

void uvm_text_tr_database::m_write_line( const std::string& line )
{
  if (open_db())
    m_file << line << '\n';
}

} // namespace uvm
