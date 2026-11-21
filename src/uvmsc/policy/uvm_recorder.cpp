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
#include <cstring>
#include <sstream>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/misc/uvm_misc.h"
#include "uvmsc/policy/uvm_recorder.h"
#include "uvmsc/policy/uvm_tr_database.h"
#include "uvmsc/policy/uvm_tr_stream.h"

using namespace sc_core;

namespace uvm {

//----------------------------------------------------------------------
// class uvm_recorder
//----------------------------------------------------------------------

std::map<int, uvm_recorder*>& uvm_recorder::m_recorders_by_id()
{
  // never deleted, so that recorders deleted at the end of the program, by
  // a database with static storage for instance, may still use it
  static std::map<int, uvm_recorder*>* recorders_by_id = new std::map<int, uvm_recorder*>();
  return *recorders_by_id;
}

uvm_recorder::uvm_recorder( const std::string& name )
: uvm_object(name),
  default_radix(UVM_HEX),
  physical(true),
  abstract(true),
  identifier(true),
  policy(UVM_DEFAULT_POLICY),
  recording_depth(0),
  m_stream(nullptr),
  m_is_opened(false),
  m_is_closed(false)
{}

uvm_recorder::~uvm_recorder()
{
  m_free_handle();
}

void uvm_recorder::set_recursion_policy( uvm_recursion_policy_enum p )
{
  policy = p;
}

uvm_recursion_policy_enum uvm_recorder::get_recursion_policy() const
{
  return policy;
}

void uvm_recorder::set_id_enabled( bool enabled )
{
  identifier = enabled;
}

bool uvm_recorder::get_id_enabled() const
{
  return identifier;
}

void uvm_recorder::set_default_radix( uvm_radix_enum radix )
{
  default_radix = radix;
}

uvm_radix_enum uvm_recorder::get_default_radix() const
{
  return default_radix;
}

uvm_tr_stream* uvm_recorder::get_stream() const
{
  if (m_stream == nullptr)
    uvm_report_warning("UVM/REC/NO_CFG", "attempt to retrieve STREAM from '" +
      get_name() + "' before it was set!", UVM_NONE);
  return m_stream;
}

//----------------------------------------------------------------------
// member function: close
//
//! Closes the recorder: nothing more is recorded. A close time of zero
//! is the current simulation time.
//----------------------------------------------------------------------

void uvm_recorder::close( const sc_time& close_time )
{
  const sc_time t = (close_time == SC_ZERO_TIME) ? sc_time_stamp() : close_time;

  if (!is_open())
    return;

  do_close(t);

  m_is_opened = false;
  m_is_closed = true;
  m_close_time = t;
}

//----------------------------------------------------------------------
// member function: free
//
//! Frees the recorder, closing it first when it is open. Other than in
//! UVM-SV, the recorder is deleted: it shall not be used afterwards.
//----------------------------------------------------------------------

void uvm_recorder::free( const sc_time& close_time )
{
  if (!is_open() && !is_closed())
    return;

  if (is_open())
    close(close_time);

  do_free();

  m_is_closed = false;
  m_free_handle();

  uvm_tr_stream* stream = m_stream;
  m_stream = nullptr;

  if (stream != nullptr)
    stream->m_free_recorder(this); // deletes this recorder
}

bool uvm_recorder::is_open() const
{
  return m_is_opened;
}

sc_time uvm_recorder::get_open_time() const
{
  return m_open_time;
}

bool uvm_recorder::is_closed() const
{
  return m_is_closed;
}

sc_time uvm_recorder::get_close_time() const
{
  return m_close_time;
}

void uvm_recorder::m_do_open( uvm_tr_stream* stream,
                              const sc_time& open_time,
                              const std::string& type_name )
{
  if (stream == nullptr)
  {
    uvm_report_error("UVM/REC/NULL_STREAM", "Illegal attempt to set STREAM for '" +
      get_name() + "' to '<null>'", UVM_NONE);
    return;
  }

  if (m_stream != nullptr)
  {
    uvm_report_error("UVM/REC/RE_INIT", "Illegal attempt to re-initialize '" +
      get_name() + "'", UVM_NONE);
    return;
  }

  m_stream = stream;
  m_open_time = open_time;
  m_is_opened = true;

  do_open(stream, open_time, type_name);
}

//----------------------------------------------------------------------
// member function: get_handle
//
//! A unique handle of the recorder while it is open or closed, and 0
//! before it is opened and after it is freed
//----------------------------------------------------------------------

int uvm_recorder::get_handle() const
{
  if (!is_open() && !is_closed())
    return 0;

  const int handle = get_inst_id();
  m_recorders_by_id()[handle] = const_cast<uvm_recorder*>(this);
  return handle;
}

uvm_recorder* uvm_recorder::get_recorder_from_handle( int id )
{
  if (id == 0)
    return nullptr;

  std::map<int, uvm_recorder*>::iterator it = m_recorders_by_id().find(id);
  return (it == m_recorders_by_id().end()) ? nullptr : it->second;
}

void uvm_recorder::m_free_handle()
{
  std::map<int, uvm_recorder*>::iterator it = m_recorders_by_id().find(get_inst_id());
  if (it != m_recorders_by_id().end() && it->second == this)
    m_recorders_by_id().erase(it);
}

//----------------------------------------------------------------------
// Attribute recording
//----------------------------------------------------------------------

void uvm_recorder::record_field( const std::string& name,
                                 const uvm_bitstream_t& value,
                                 int size,
                                 uvm_radix_enum radix )
{
  if (get_stream() == nullptr)
    return;
  do_record_field(name, value, size, radix);
}

void uvm_recorder::record_field_int( const std::string& name,
                                     const uvm_integral_t& value,
                                     int size,
                                     uvm_radix_enum radix )
{
  if (get_stream() == nullptr)
    return;
  do_record_field_int(name, value, size, radix);
}

void uvm_recorder::record_field_real( const std::string& name, double value )
{
  if (get_stream() == nullptr)
    return;
  do_record_field_real(name, value);
}

//----------------------------------------------------------------------
// member function: record_object
//
//! Records an object, and with the recursion policy UVM_REFERENCE only
//! its reference. An object which is being recorded already, one which
//! refers to itself, is not recorded again.
//----------------------------------------------------------------------

void uvm_recorder::record_object( const std::string& name, uvm_object* value )
{
  if (get_stream() == nullptr)
    return;

  if (value == nullptr)
  {
    do_record_object(name, value);
    return;
  }

  if (std::find(m_active_objects.begin(), m_active_objects.end(), value) != m_active_objects.end())
    return;

  m_active_objects.push_back(value);
  recording_depth++;
  do_record_object(name, value);
  recording_depth--;
  m_active_objects.pop_back();
}

void uvm_recorder::record_string( const std::string& name, const std::string& value )
{
  if (get_stream() == nullptr)
    return;
  do_record_string(name, value);
}

void uvm_recorder::record_time( const std::string& name, const sc_time& value )
{
  if (get_stream() == nullptr)
    return;
  do_record_time(name, value);
}

void uvm_recorder::record_generic( const std::string& name,
                                   const std::string& value,
                                   const std::string& type_name )
{
  if (get_stream() == nullptr)
    return;
  do_record_generic(name, value, type_name);
}

bool uvm_recorder::use_record_attribute() const
{
  return false;
}

int uvm_recorder::get_record_attribute_handle() const
{
  return get_handle();
}

int uvm_recorder::get_active_object_depth() const
{
  return static_cast<int>(m_active_objects.size());
}

//----------------------------------------------------------------------
// Implementation agnostic API: this base class stores nothing
//----------------------------------------------------------------------

void uvm_recorder::do_open( uvm_tr_stream*, const sc_time&, const std::string& ) {}
void uvm_recorder::do_close( const sc_time& ) {}
void uvm_recorder::do_free() {}

void uvm_recorder::do_record_field( const std::string&, const uvm_bitstream_t&, int, uvm_radix_enum ) {}
void uvm_recorder::do_record_field_int( const std::string&, const uvm_integral_t&, int, uvm_radix_enum ) {}
void uvm_recorder::do_record_field_real( const std::string&, double ) {}

//! Records the fields of the object through its do_record(), unless the
//! recursion policy is UVM_REFERENCE
void uvm_recorder::do_record_object( const std::string&, uvm_object* value )
{
  if ((get_recursion_policy() != UVM_REFERENCE) && (value != nullptr))
    value->do_record(*this);
}

void uvm_recorder::do_record_string( const std::string&, const std::string& ) {}
void uvm_recorder::do_record_time( const std::string&, const sc_time& ) {}
void uvm_recorder::do_record_generic( const std::string&, const std::string&, const std::string& ) {}

//----------------------------------------------------------------------
// class uvm_text_recorder
//----------------------------------------------------------------------

uvm_text_recorder::uvm_text_recorder( const std::string& name )
: uvm_recorder(name), m_text_db(nullptr)
{}

void uvm_text_recorder::do_open( uvm_tr_stream* stream,
                                 const sc_time& open_time,
                                 const std::string& type_name )
{
  m_text_db = dynamic_cast<uvm_text_tr_database*>(stream->get_db());

  if (m_text_db != nullptr && m_text_db->open_db())
  {
    std::ostringstream str;
    str << "    OPEN_RECORDER @" << sc_time_stamp()
        << " {TXH:" << get_handle()
        << " STREAM:" << stream->get_handle()
        << " NAME:" << get_name()
        << " TIME:" << open_time
        << " TYPE=\"" << type_name << "\"}";
    m_text_db->m_write_line(str.str());
  }
}

void uvm_text_recorder::do_close( const sc_time& close_time )
{
  if (m_text_db != nullptr && m_text_db->open_db())
  {
    std::ostringstream str;
    str << "    CLOSE_RECORDER @" << sc_time_stamp()
        << " {TXH:" << get_handle()
        << " TIME=" << close_time << "}";
    m_text_db->m_write_line(str.str());
  }
}

void uvm_text_recorder::do_free()
{
  if (m_text_db != nullptr && m_text_db->open_db())
  {
    std::ostringstream str;
    str << "    FREE_RECORDER @" << sc_time_stamp()
        << " {TXH:" << get_handle() << "}";
    m_text_db->m_write_line(str.str());
  }
  m_text_db = nullptr;
}

void uvm_text_recorder::do_record_field( const std::string& name,
                                         const uvm_bitstream_t& value,
                                         int size,
                                         uvm_radix_enum radix )
{
  if (radix == UVM_NORADIX)
    radix = get_default_radix();

  write_attribute(m_current_context(name), value, radix, size);
}

void uvm_text_recorder::do_record_field_int( const std::string& name,
                                             const uvm_integral_t& value,
                                             int size,
                                             uvm_radix_enum radix )
{
  if (radix == UVM_NORADIX)
    radix = get_default_radix();

  write_attribute_int(m_current_context(name), value, radix, size);
}

void uvm_text_recorder::do_record_field_real( const std::string& name, double value )
{
  std::ostringstream str;
  str << value;
  m_write_attr(m_current_context(name), str.str(), "UVM_REAL", 64);
}

std::string uvm_text_recorder::m_current_context( const std::string& name ) const
{
  if (m_object_names.empty())
    return name;

  std::string full_name;
  for (unsigned int i = 0; i < m_object_names.size(); i++)
    full_name += (i == 0 ? "" : ".") + m_object_names[i];

  return name.empty() ? full_name : full_name + "." + name;
}

void uvm_text_recorder::do_record_object( const std::string& name, uvm_object* value )
{
  if (get_id_enabled())
    write_attribute_int("inst_id", (value != nullptr) ? value->get_inst_id() : 0, UVM_DEC, 32);

  if (get_active_object_depth() > 1)
    m_object_names.push_back(name);

  uvm_recorder::do_record_object(name, value);

  if (get_active_object_depth() > 1)
    m_object_names.pop_back();
}

void uvm_text_recorder::do_record_string( const std::string& name, const std::string& value )
{
  m_write_attr(m_current_context(name), value, "UVM_STRING",
    8 + static_cast<int>(value.size()));
}

void uvm_text_recorder::do_record_time( const std::string& name, const sc_time& value )
{
  m_write_attr(m_current_context(name), value.to_string(), "UVM_TIME", 64);
}

void uvm_text_recorder::do_record_generic( const std::string& name,
                                           const std::string& value,
                                           const std::string& )
{
  m_write_attr(m_current_context(name), value, "UVM_STRING",
    8 + static_cast<int>(value.size()));
}

void uvm_text_recorder::write_attribute( const std::string& nm,
                                         const uvm_bitstream_t& value,
                                         uvm_radix_enum radix,
                                         int numbits )
{
  m_write_attr(nm, uvm_vector_to_string(value, numbits, radix), uvm_radix_enum_name[radix], numbits);
}

void uvm_text_recorder::write_attribute_int( const std::string& nm,
                                             const uvm_integral_t& value,
                                             uvm_radix_enum radix,
                                             int numbits )
{
  m_write_attr(nm, uvm_vector_to_string(value, numbits, radix), uvm_radix_enum_name[radix], numbits);
}

void uvm_text_recorder::m_write_attr( const std::string& nm, const std::string& value,
                                      const std::string& radix, int numbits )
{
  if (m_text_db != nullptr && m_text_db->open_db())
  {
    std::ostringstream str;
    str << "      SET_ATTR @" << sc_time_stamp()
        << " {TXH:" << get_handle()
        << " NAME:" << nm
        << " VALUE:" << value
        << "   RADIX:" << radix
        << " BITS=" << numbits << "}";
    m_text_db->m_write_line(str.str());
  }
}

} // namespace uvm
