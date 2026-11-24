//----------------------------------------------------------------------
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
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "uvmsc/tlm2/uvm_tlm_gp.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/policy/uvm_comparer.h"
#include "uvmsc/policy/uvm_packer.h"
#include "uvmsc/policy/uvm_recorder.h"
#include "uvmsc/print/uvm_printer.h"

namespace uvm {

// Verify 1:1 enum alignment with SystemC TLM-2.0
static_assert(static_cast<int>(UVM_TLM_READ_COMMAND)   == static_cast<int>(tlm::TLM_READ_COMMAND),   "command READ mismatch");
static_assert(static_cast<int>(UVM_TLM_WRITE_COMMAND)  == static_cast<int>(tlm::TLM_WRITE_COMMAND),  "command WRITE mismatch");
static_assert(static_cast<int>(UVM_TLM_IGNORE_COMMAND) == static_cast<int>(tlm::TLM_IGNORE_COMMAND), "command IGNORE mismatch");

static_assert(static_cast<int>(UVM_TLM_OK_RESPONSE)                == static_cast<int>(tlm::TLM_OK_RESPONSE),                "resp OK mismatch");
static_assert(static_cast<int>(UVM_TLM_INCOMPLETE_RESPONSE)        == static_cast<int>(tlm::TLM_INCOMPLETE_RESPONSE),        "resp INCOMPLETE mismatch");
static_assert(static_cast<int>(UVM_TLM_GENERIC_ERROR_RESPONSE)     == static_cast<int>(tlm::TLM_GENERIC_ERROR_RESPONSE),     "resp GENERIC_ERROR mismatch");
static_assert(static_cast<int>(UVM_TLM_ADDRESS_ERROR_RESPONSE)     == static_cast<int>(tlm::TLM_ADDRESS_ERROR_RESPONSE),     "resp ADDRESS_ERROR mismatch");
static_assert(static_cast<int>(UVM_TLM_COMMAND_ERROR_RESPONSE)     == static_cast<int>(tlm::TLM_COMMAND_ERROR_RESPONSE),     "resp COMMAND_ERROR mismatch");
static_assert(static_cast<int>(UVM_TLM_BURST_ERROR_RESPONSE)       == static_cast<int>(tlm::TLM_BURST_ERROR_RESPONSE),       "resp BURST_ERROR mismatch");
static_assert(static_cast<int>(UVM_TLM_BYTE_ENABLE_ERROR_RESPONSE) == static_cast<int>(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE), "resp BYTE_ENABLE_ERROR mismatch");

static_assert(static_cast<int>(UVM_TLM_MIN_PAYLOAD)           == static_cast<int>(tlm::TLM_MIN_PAYLOAD),           "gp_option MIN mismatch");
static_assert(static_cast<int>(UVM_TLM_FULL_PAYLOAD)          == static_cast<int>(tlm::TLM_FULL_PAYLOAD),          "gp_option FULL mismatch");
static_assert(static_cast<int>(UVM_TLM_FULL_PAYLOAD_ACCEPTED) == static_cast<int>(tlm::TLM_FULL_PAYLOAD_ACCEPTED), "gp_option ACCEPTED mismatch");

static_assert(static_cast<int>(UNINITIALIZED_PHASE) == static_cast<int>(tlm::UNINITIALIZED_PHASE), "phase UNINIT mismatch");
static_assert(static_cast<int>(BEGIN_REQ)           == static_cast<int>(tlm::BEGIN_REQ),           "phase BEGIN_REQ mismatch");
static_assert(static_cast<int>(END_REQ)             == static_cast<int>(tlm::END_REQ),             "phase END_REQ mismatch");
static_assert(static_cast<int>(BEGIN_RESP)          == static_cast<int>(tlm::BEGIN_RESP),          "phase BEGIN_RESP mismatch");
static_assert(static_cast<int>(END_RESP)            == static_cast<int>(tlm::END_RESP),            "phase END_RESP mismatch");

static_assert(static_cast<int>(UVM_TLM_ACCEPTED)  == static_cast<int>(tlm::TLM_ACCEPTED),  "sync ACCEPTED mismatch");
static_assert(static_cast<int>(UVM_TLM_UPDATED)   == static_cast<int>(tlm::TLM_UPDATED),   "sync UPDATED mismatch");
static_assert(static_cast<int>(UVM_TLM_COMPLETED) == static_cast<int>(tlm::TLM_COMPLETED), "sync COMPLETED mismatch");

static_assert(UVM_TLM_BYTE_DISABLED == TLM_BYTE_DISABLED, "TLM_BYTE_DISABLED mismatch");
static_assert(UVM_TLM_BYTE_ENABLED  == TLM_BYTE_ENABLED,  "TLM_BYTE_ENABLED mismatch");

//----------------------------------------------------------------------
// uvm_tlm_extension_base
//----------------------------------------------------------------------

uvm_tlm_extension_base::uvm_tlm_extension_base( const std::string& name )
  : uvm_object(name)
{}

uvm_tlm_extension_base::uvm_tlm_extension_base( const std::string& name, no_inst_count_t )
  : uvm_object(name, no_inst_count_t())
{}

uvm_tlm_extension_base::~uvm_tlm_extension_base()
{}

uvm_object* uvm_tlm_extension_base::create( const std::string& name )
{
  (void)name;
  return nullptr;
}

void uvm_tlm_extension_base::do_copy( const uvm_object& rhs )
{
  (void)rhs;
}

void uvm_tlm_extension_base::copy_from( const uvm_tlm_extension_base& rhs )
{
  copy(rhs);
}

//----------------------------------------------------------------------
// uvm_tlm_generic_payload
//----------------------------------------------------------------------

uvm_tlm_generic_payload::uvm_tlm_generic_payload( const std::string& name )
  : uvm_sequence_item(name),
    m_address(0),
    m_command(UVM_TLM_IGNORE_COMMAND),
    m_data(),
    m_length(0),
    m_response_status(UVM_TLM_INCOMPLETE_RESPONSE),
    m_dmi(false),
    m_byte_enable(),
    m_byte_enable_length(0),
    m_streaming_width(0),
    m_gp_option(UVM_TLM_MIN_PAYLOAD),
    m_extensions()
{}

uvm_tlm_generic_payload::uvm_tlm_generic_payload( const uvm_tlm_generic_payload& rhs )
  : uvm_sequence_item(rhs),
    m_address(0),
    m_command(UVM_TLM_IGNORE_COMMAND),
    m_data(),
    m_length(0),
    m_response_status(UVM_TLM_INCOMPLETE_RESPONSE),
    m_dmi(false),
    m_byte_enable(),
    m_byte_enable_length(0),
    m_streaming_width(0),
    m_gp_option(UVM_TLM_MIN_PAYLOAD),
    m_extensions()
{
  m_copy_gp_fields(rhs);
}

uvm_tlm_generic_payload& uvm_tlm_generic_payload::operator=( const uvm_tlm_generic_payload& rhs )
{
  if (this != &rhs)
  {
    uvm_sequence_item::operator=(rhs);
    m_copy_gp_fields(rhs);
  }
  return *this;
}

uvm_tlm_generic_payload::~uvm_tlm_generic_payload()
{
  clear_extensions();
}

uvm_object* uvm_tlm_generic_payload::create( const std::string& name )
{
  return new uvm_tlm_generic_payload(name);
}

void uvm_tlm_generic_payload::m_copy_gp_fields( const uvm_tlm_generic_payload& rhs )
{
  if (this == &rhs)
    return;

  m_address = rhs.m_address;
  m_command = rhs.m_command;
  m_data = rhs.m_data;
  m_length = rhs.m_length;
  m_response_status = rhs.m_response_status;
  m_dmi = rhs.m_dmi;
  m_byte_enable = rhs.m_byte_enable;
  m_byte_enable_length = rhs.m_byte_enable_length;
  m_streaming_width = rhs.m_streaming_width;
  m_gp_option = rhs.m_gp_option;

  clear_extensions();
  for (std::map<const uvm_tlm_extension_base*, std::pair<uvm_tlm_extension_base*, bool> >::const_iterator it =
         rhs.m_extensions.begin();
       it != rhs.m_extensions.end();
       ++it)
  {
    if (it->second.first != nullptr)
    {
      uvm_tlm_extension_base* cloned =
        dynamic_cast<uvm_tlm_extension_base*>(it->second.first->clone());
      if (cloned != nullptr)
        m_extensions[it->first] = std::make_pair(cloned, true);
    }
  }
}

bool uvm_tlm_generic_payload::is_read() const
{
  return (m_command == UVM_TLM_READ_COMMAND);
}

void uvm_tlm_generic_payload::set_read()
{
  m_command = UVM_TLM_READ_COMMAND;
}

bool uvm_tlm_generic_payload::is_write() const
{
  return (m_command == UVM_TLM_WRITE_COMMAND);
}

void uvm_tlm_generic_payload::set_write()
{
  m_command = UVM_TLM_WRITE_COMMAND;
}

uvm_tlm_command_e uvm_tlm_generic_payload::get_command() const
{
  return m_command;
}

void uvm_tlm_generic_payload::set_command( uvm_tlm_command_e command )
{
  m_command = command;
}

sc_dt::uint64 uvm_tlm_generic_payload::get_address() const
{
  return m_address;
}

void uvm_tlm_generic_payload::set_address( sc_dt::uint64 addr )
{
  m_address = addr;
}

void uvm_tlm_generic_payload::get_data( std::vector<unsigned char>& p ) const
{
  p = m_data;
}

void uvm_tlm_generic_payload::set_data( const std::vector<unsigned char>& p )
{
  m_data = p;
}

unsigned char* uvm_tlm_generic_payload::get_data_ptr()
{
  return m_data.empty() ? nullptr : &m_data[0];
}

const unsigned char* uvm_tlm_generic_payload::get_data_ptr() const
{
  return m_data.empty() ? nullptr : &m_data[0];
}

unsigned int uvm_tlm_generic_payload::get_data_length() const
{
  return m_length;
}

void uvm_tlm_generic_payload::set_data_length( unsigned int length )
{
  m_length = length;
}

bool uvm_tlm_generic_payload::is_response_ok() const
{
  return (static_cast<int>(m_response_status) > 0);
}

bool uvm_tlm_generic_payload::is_response_error() const
{
  return (static_cast<int>(m_response_status) <= 0);
}

uvm_tlm_response_status_e uvm_tlm_generic_payload::get_response_status() const
{
  return m_response_status;
}

void uvm_tlm_generic_payload::set_response_status( uvm_tlm_response_status_e response_status )
{
  m_response_status = response_status;
}

std::string uvm_tlm_generic_payload::get_response_string() const
{
  switch (m_response_status)
  {
    case UVM_TLM_OK_RESPONSE:                return "UVM_TLM_OK_RESPONSE";
    case UVM_TLM_INCOMPLETE_RESPONSE:        return "UVM_TLM_INCOMPLETE_RESPONSE";
    case UVM_TLM_GENERIC_ERROR_RESPONSE:     return "UVM_TLM_GENERIC_ERROR_RESPONSE";
    case UVM_TLM_ADDRESS_ERROR_RESPONSE:     return "UVM_TLM_ADDRESS_ERROR_RESPONSE";
    case UVM_TLM_COMMAND_ERROR_RESPONSE:     return "UVM_TLM_COMMAND_ERROR_RESPONSE";
    case UVM_TLM_BURST_ERROR_RESPONSE:       return "UVM_TLM_BURST_ERROR_RESPONSE";
    case UVM_TLM_BYTE_ENABLE_ERROR_RESPONSE: return "UVM_TLM_BYTE_ENABLE_ERROR_RESPONSE";
    default:                                 return "UNKNOWN";
  }
}

unsigned int uvm_tlm_generic_payload::get_streaming_width() const
{
  return m_streaming_width;
}

void uvm_tlm_generic_payload::set_streaming_width( unsigned int streaming_width )
{
  m_streaming_width = streaming_width;
}

void uvm_tlm_generic_payload::get_byte_enable( std::vector<unsigned char>& p ) const
{
  p = m_byte_enable;
}

void uvm_tlm_generic_payload::set_byte_enable( const std::vector<unsigned char>& p )
{
  m_byte_enable = p;
}

unsigned char* uvm_tlm_generic_payload::get_byte_enable_ptr()
{
  return m_byte_enable.empty() ? nullptr : &m_byte_enable[0];
}

const unsigned char* uvm_tlm_generic_payload::get_byte_enable_ptr() const
{
  return m_byte_enable.empty() ? nullptr : &m_byte_enable[0];
}

unsigned int uvm_tlm_generic_payload::get_byte_enable_length() const
{
  return m_byte_enable_length;
}

void uvm_tlm_generic_payload::set_byte_enable_length( unsigned int byte_enable_length )
{
  m_byte_enable_length = byte_enable_length;
}

void uvm_tlm_generic_payload::set_dmi_allowed( bool dmi_allowed )
{
  m_dmi = dmi_allowed;
}

bool uvm_tlm_generic_payload::is_dmi_allowed() const
{
  return m_dmi;
}

uvm_tlm_gp_option_e uvm_tlm_generic_payload::get_gp_option() const
{
  return m_gp_option;
}

void uvm_tlm_generic_payload::set_gp_option( uvm_tlm_gp_option_e gp_opt )
{
  m_gp_option = gp_opt;
}

void uvm_tlm_generic_payload::reset()
{
  m_address = 0;
  m_command = UVM_TLM_IGNORE_COMMAND;
  m_data.clear();
  m_length = 0;
  m_response_status = UVM_TLM_INCOMPLETE_RESPONSE;
  m_dmi = false;
  m_byte_enable.clear();
  m_byte_enable_length = 0;
  m_streaming_width = 0;
  m_gp_option = UVM_TLM_MIN_PAYLOAD;
  clear_extensions();
}

uvm_tlm_extension_base* uvm_tlm_generic_payload::set_extension( uvm_tlm_extension_base* ext )
{
  if (ext == nullptr)
    return nullptr;

  const uvm_tlm_extension_base* key = ext->get_type_handle();
  if (key == nullptr)
    return nullptr;

  uvm_tlm_extension_base* prev = nullptr;
  std::map<const uvm_tlm_extension_base*, std::pair<uvm_tlm_extension_base*, bool> >::iterator it =
    m_extensions.find(key);
  if (it != m_extensions.end())
    prev = it->second.first;

  m_extensions[key] = std::make_pair(ext, false);
  return prev;
}

uvm_tlm_extension_base* uvm_tlm_generic_payload::get_extension(
  const uvm_tlm_extension_base* ext_handle ) const
{
  if (ext_handle == nullptr)
    return nullptr;

  const uvm_tlm_extension_base* key = ext_handle->get_type_handle();
  std::map<const uvm_tlm_extension_base*, std::pair<uvm_tlm_extension_base*, bool> >::const_iterator it =
    m_extensions.find(key);
  if (it != m_extensions.end())
    return it->second.first;
  return nullptr;
}

void uvm_tlm_generic_payload::clear_extension( const uvm_tlm_extension_base* ext_handle )
{
  if (ext_handle == nullptr)
    return;

  const uvm_tlm_extension_base* key = ext_handle->get_type_handle();
  std::map<const uvm_tlm_extension_base*, std::pair<uvm_tlm_extension_base*, bool> >::iterator it =
    m_extensions.find(key);
  if (it != m_extensions.end())
  {
    if (it->second.second && it->second.first != nullptr)
      delete it->second.first;
    m_extensions.erase(it);
  }
}

void uvm_tlm_generic_payload::clear_extensions()
{
  for (std::map<const uvm_tlm_extension_base*, std::pair<uvm_tlm_extension_base*, bool> >::iterator it =
         m_extensions.begin();
       it != m_extensions.end();
       ++it)
  {
    if (it->second.second && it->second.first != nullptr)
      delete it->second.first;
  }
  m_extensions.clear();
}

unsigned int uvm_tlm_generic_payload::get_num_extensions() const
{
  return static_cast<unsigned int>(m_extensions.size());
}

void uvm_tlm_generic_payload::to_tlm_gp( tlm::tlm_generic_payload& dst,
                                          unsigned char* data_buf,
                                          unsigned char* be_buf ) const
{
  dst.set_command(static_cast<tlm::tlm_command>(m_command));
  dst.set_address(m_address);
  dst.set_data_length(m_length);
  dst.set_streaming_width(m_streaming_width);
  dst.set_byte_enable_length(m_byte_enable_length);
  dst.set_response_status(static_cast<tlm::tlm_response_status>(m_response_status));
  dst.set_dmi_allowed(m_dmi);
  dst.set_gp_option(static_cast<tlm::tlm_gp_option>(m_gp_option));

  if (data_buf != nullptr)
  {
    const std::size_t copy_n = std::min(static_cast<std::size_t>(m_length), m_data.size());
    if (copy_n > 0)
      std::memcpy(data_buf, &m_data[0], copy_n);
    dst.set_data_ptr(data_buf);
  }
  else if (dst.get_data_ptr() != nullptr)
  {
    const std::size_t copy_n = std::min(static_cast<std::size_t>(m_length), m_data.size());
    if (copy_n > 0)
      std::memcpy(dst.get_data_ptr(), &m_data[0], copy_n);
  }
  else
  {
    dst.set_data_ptr(m_data.empty() ? nullptr : const_cast<unsigned char*>(&m_data[0]));
  }

  if (be_buf != nullptr)
  {
    const std::size_t copy_n =
      std::min(static_cast<std::size_t>(m_byte_enable_length), m_byte_enable.size());
    if (copy_n > 0)
      std::memcpy(be_buf, &m_byte_enable[0], copy_n);
    dst.set_byte_enable_ptr(be_buf);
  }
  else if (dst.get_byte_enable_ptr() != nullptr)
  {
    const std::size_t copy_n =
      std::min(static_cast<std::size_t>(m_byte_enable_length), m_byte_enable.size());
    if (copy_n > 0)
      std::memcpy(dst.get_byte_enable_ptr(), &m_byte_enable[0], copy_n);
  }
  else
  {
    dst.set_byte_enable_ptr(
      m_byte_enable.empty() ? nullptr : const_cast<unsigned char*>(&m_byte_enable[0]));
  }
}

void uvm_tlm_generic_payload::to_tlm_gp_in_place( tlm::tlm_generic_payload& dst ) const
{
  const unsigned int orig_data_len = dst.get_data_length();
  const unsigned int orig_be_len = dst.get_byte_enable_length();

  dst.set_command(static_cast<tlm::tlm_command>(m_command));
  dst.set_address(m_address);
  dst.set_streaming_width(m_streaming_width);
  dst.set_response_status(static_cast<tlm::tlm_response_status>(m_response_status));
  dst.set_dmi_allowed(m_dmi);
  dst.set_gp_option(static_cast<tlm::tlm_gp_option>(m_gp_option));

  if (dst.get_data_ptr() != nullptr)
  {
    const unsigned int clamped_data_len = std::min(orig_data_len, m_length);
    dst.set_data_length(clamped_data_len);
    const std::size_t copy_n = std::min(static_cast<std::size_t>(clamped_data_len), m_data.size());
    if (copy_n > 0)
      std::memcpy(dst.get_data_ptr(), &m_data[0], copy_n);
  }
  else
  {
    dst.set_data_length(m_length);
  }

  if (dst.get_byte_enable_ptr() != nullptr)
  {
    const unsigned int clamped_be_len = std::min(orig_be_len, m_byte_enable_length);
    dst.set_byte_enable_length(clamped_be_len);
    const std::size_t copy_n = std::min(static_cast<std::size_t>(clamped_be_len), m_byte_enable.size());
    if (copy_n > 0)
      std::memcpy(dst.get_byte_enable_ptr(), &m_byte_enable[0], copy_n);
  }
  else
  {
    dst.set_byte_enable_length(0);
  }
}

void uvm_tlm_generic_payload::from_tlm_gp( const tlm::tlm_generic_payload& src )
{
  m_command = static_cast<uvm_tlm_command_e>(src.get_command());
  m_address = src.get_address();
  m_length = src.get_data_length();
  m_streaming_width = src.get_streaming_width();
  m_byte_enable_length = src.get_byte_enable_length();
  m_response_status = static_cast<uvm_tlm_response_status_e>(src.get_response_status());
  m_dmi = src.is_dmi_allowed();
  m_gp_option = static_cast<uvm_tlm_gp_option_e>(src.get_gp_option());

  const unsigned char* dptr = src.get_data_ptr();
  if (dptr != nullptr && m_length > 0)
    m_data.assign(dptr, dptr + m_length);
  else
    m_data.clear();

  const unsigned char* bptr = src.get_byte_enable_ptr();
  if (bptr != nullptr && m_byte_enable_length > 0)
    m_byte_enable.assign(bptr, bptr + m_byte_enable_length);
  else
    m_byte_enable.clear();
}

void uvm_tlm_generic_payload::do_copy( const uvm_object& rhs )
{
  const uvm_tlm_generic_payload* rhs_gp =
    dynamic_cast<const uvm_tlm_generic_payload*>(&rhs);
  if (rhs_gp == nullptr)
  {
    UVM_FATAL("UVM/TLM2/BADCOPY",
              "do_copy: rhs is not of type uvm_tlm_generic_payload");
    return;
  }
  m_copy_gp_fields(*rhs_gp);
}

bool uvm_tlm_generic_payload::do_compare( const uvm_object& rhs,
                                          const uvm_comparer* comparer ) const
{
  const uvm_comparer* eff =
    (comparer != nullptr) ? comparer : uvm_comparer::get_default();
  const uvm_tlm_generic_payload* rhs_gp =
    dynamic_cast<const uvm_tlm_generic_payload*>(&rhs);
  if (rhs_gp == nullptr)
  {
    eff->print_msg("rhs is not of type uvm_tlm_generic_payload");
    return false;
  }

  bool ok = true;
  ok &= eff->compare_field_int("address", m_address, rhs_gp->m_address, 64, UVM_HEX);
  ok &= eff->compare_field_int("command",
                               static_cast<uvm_integral_t>(m_command),
                               static_cast<uvm_integral_t>(rhs_gp->m_command),
                               32, UVM_DEC);
  ok &= eff->compare_field_int("data_length", m_length, rhs_gp->m_length, 32, UVM_DEC);

  if (m_length == rhs_gp->m_length)
  {
    const std::size_t n_data = std::min(static_cast<std::size_t>(m_length),
                                        std::min(m_data.size(), rhs_gp->m_data.size()));
    for (std::size_t i = 0; i < n_data; ++i)
    {
      std::ostringstream idx;
      idx << "data[" << i << "]";
      ok &= eff->compare_field_int(idx.str(), m_data[i], rhs_gp->m_data[i], 8, UVM_HEX);
    }
    const bool lhs_has = (m_data.size() >= m_length);
    const bool rhs_has = (rhs_gp->m_data.size() >= rhs_gp->m_length);
    if ((!lhs_has || !rhs_has) && (m_data.size() != rhs_gp->m_data.size()))
      ok &= eff->compare_field_int("data_buf_size", m_data.size(), rhs_gp->m_data.size(), 32, UVM_DEC);
  }

  ok &= eff->compare_field_int("response_status",
                               static_cast<uvm_integral_t>(static_cast<int>(m_response_status)),
                               static_cast<uvm_integral_t>(static_cast<int>(rhs_gp->m_response_status)),
                               32, UVM_DEC);
  ok &= eff->compare_field_int("dmi_allowed",
                               m_dmi ? 1 : 0, rhs_gp->m_dmi ? 1 : 0, 1, UVM_BIN);
  ok &= eff->compare_field_int("byte_enable_length",
                               m_byte_enable_length, rhs_gp->m_byte_enable_length, 32, UVM_DEC);

  if (m_byte_enable_length == rhs_gp->m_byte_enable_length)
  {
    const std::size_t n_be = std::min(static_cast<std::size_t>(m_byte_enable_length),
                                      std::min(m_byte_enable.size(), rhs_gp->m_byte_enable.size()));
    for (std::size_t i = 0; i < n_be; ++i)
    {
      std::ostringstream idx;
      idx << "byte_enable[" << i << "]";
      ok &= eff->compare_field_int(idx.str(), m_byte_enable[i], rhs_gp->m_byte_enable[i], 8, UVM_HEX);
    }
    const bool lhs_has_be = (m_byte_enable.size() >= m_byte_enable_length);
    const bool rhs_has_be = (rhs_gp->m_byte_enable.size() >= rhs_gp->m_byte_enable_length);
    if ((!lhs_has_be || !rhs_has_be) && (m_byte_enable.size() != rhs_gp->m_byte_enable.size()))
      ok &= eff->compare_field_int("byte_enable_buf_size",
                                   m_byte_enable.size(),
                                   rhs_gp->m_byte_enable.size(),
                                   32, UVM_DEC);
  }

  ok &= eff->compare_field_int("streaming_width",
                               m_streaming_width, rhs_gp->m_streaming_width, 32, UVM_DEC);
  ok &= eff->compare_field_int("gp_option",
                               static_cast<uvm_integral_t>(m_gp_option),
                               static_cast<uvm_integral_t>(rhs_gp->m_gp_option),
                               32, UVM_DEC);

  ok &= eff->compare_field_int("num_extensions",
                               m_extensions.size(), rhs_gp->m_extensions.size(), 32, UVM_DEC);

  for (std::map<const uvm_tlm_extension_base*, std::pair<uvm_tlm_extension_base*, bool> >::const_iterator it =
         m_extensions.begin();
       it != m_extensions.end();
       ++it)
  {
    std::map<const uvm_tlm_extension_base*, std::pair<uvm_tlm_extension_base*, bool> >::const_iterator rhs_it =
      rhs_gp->m_extensions.find(it->first);
    if (rhs_it == rhs_gp->m_extensions.end())
    {
      eff->print_msg("Missing extension in rhs: " +
                     (it->second.first ? it->second.first->get_type_handle_name() : std::string("unknown")));
      ok = false;
    }
    else
    {
      const std::string ext_nm =
        it->second.first ? it->second.first->get_type_handle_name() : std::string("extension");
      ok &= eff->compare_object(ext_nm, it->second.first, rhs_it->second.first);
    }
  }

  return ok;
}

std::string uvm_tlm_generic_payload::m_command_string() const
{
  switch (m_command)
  {
    case UVM_TLM_READ_COMMAND:   return "UVM_TLM_READ_COMMAND";
    case UVM_TLM_WRITE_COMMAND:  return "UVM_TLM_WRITE_COMMAND";
    case UVM_TLM_IGNORE_COMMAND: return "UVM_TLM_IGNORE_COMMAND";
    default:                     return "UNKNOWN";
  }
}

std::string uvm_tlm_generic_payload::m_option_string() const
{
  switch (m_gp_option)
  {
    case UVM_TLM_MIN_PAYLOAD:           return "UVM_TLM_MIN_PAYLOAD";
    case UVM_TLM_FULL_PAYLOAD:          return "UVM_TLM_FULL_PAYLOAD";
    case UVM_TLM_FULL_PAYLOAD_ACCEPTED: return "UVM_TLM_FULL_PAYLOAD_ACCEPTED";
    default:                            return "UNKNOWN";
  }
}

void uvm_tlm_generic_payload::do_print( const uvm_printer& printer ) const
{
  uvm_sequence_item::do_print(printer);

  printer.print_field_int("address", m_address, 64, UVM_HEX);
  printer.print_string("command", m_command_string());
  printer.print_field_int("data_length", m_length, 32, UVM_DEC);

  const int d_sz = static_cast<int>(m_data.size());
  printer.print_array_header("data", d_sz, "da(byte)");
  for (int i = 0; i < d_sz; ++i)
  {
    if (printer.knobs.begin_elements != -1 &&
        i >= printer.knobs.begin_elements &&
        i < d_sz - printer.knobs.end_elements)
    {
      if (i == printer.knobs.begin_elements)
        printer.print_array_range(printer.knobs.begin_elements, d_sz - printer.knobs.end_elements - 1);
      continue;
    }
    std::ostringstream idx;
    idx << "[" << i << "]";
    printer.print_field_int(idx.str(), m_data[i], 8, UVM_HEX);
  }
  printer.print_array_footer(d_sz);

  printer.print_string("response_status", get_response_string());
  printer.print_field_int("dmi_allowed", m_dmi ? 1 : 0, 1, UVM_BIN);
  printer.print_field_int("byte_enable_length", m_byte_enable_length, 32, UVM_DEC);

  const int be_sz = static_cast<int>(m_byte_enable.size());
  printer.print_array_header("byte_enable", be_sz, "da(byte)");
  for (int i = 0; i < be_sz; ++i)
  {
    std::ostringstream idx;
    idx << "[" << i << "]";
    printer.print_field_int(idx.str(), m_byte_enable[i], 8, UVM_HEX);
  }
  printer.print_array_footer(be_sz);

  printer.print_field_int("streaming_width", m_streaming_width, 32, UVM_DEC);
  printer.print_string("gp_option", m_option_string());

  if (!m_extensions.empty())
  {
    const int ext_sz = static_cast<int>(m_extensions.size());
    printer.print_array_header("extensions", ext_sz, "aa(ext)");
    for (std::map<const uvm_tlm_extension_base*, std::pair<uvm_tlm_extension_base*, bool> >::const_iterator it =
           m_extensions.begin();
         it != m_extensions.end();
         ++it)
    {
      if (it->second.first != nullptr)
        printer.print_object(it->second.first->get_type_handle_name(), *(it->second.first));
    }
    printer.print_array_footer(ext_sz);
  }
}

void uvm_tlm_generic_payload::do_pack( uvm_packer& packer ) const
{
  packer.pack_field_int(m_address, 64);
  packer.pack_field_int(static_cast<uvm_integral_t>(static_cast<int>(m_command)), 32);
  packer.pack_field_int(m_length, 32);
  for (unsigned int i = 0; i < m_length; ++i)
  {
    const unsigned char b = (i < m_data.size()) ? m_data[i] : 0;
    packer.pack_field_int(b, 8);
  }
  packer.pack_field_int(static_cast<uvm_integral_t>(static_cast<int>(m_response_status)), 32);
  packer.pack_field_int(m_dmi ? 1 : 0, 1);
  packer.pack_field_int(m_byte_enable_length, 32);
  for (unsigned int i = 0; i < m_byte_enable_length; ++i)
  {
    const unsigned char b = (i < m_byte_enable.size()) ? m_byte_enable[i] : 0;
    packer.pack_field_int(b, 8);
  }
  packer.pack_field_int(m_streaming_width, 32);
  packer.pack_field_int(static_cast<uvm_integral_t>(static_cast<int>(m_gp_option)), 32);
}

void uvm_tlm_generic_payload::do_unpack( uvm_packer& packer )
{
  m_address = packer.unpack_field_int(64).to_uint64();
  m_command = static_cast<uvm_tlm_command_e>(
    static_cast<std::int32_t>(packer.unpack_field_int(32).to_uint()));
  m_length = packer.unpack_field_int(32).to_uint();

  m_data.clear();
  m_data.reserve(std::min<unsigned int>(m_length, 4096u));
  for (unsigned int i = 0; i < m_length; ++i)
    m_data.push_back(static_cast<unsigned char>(packer.unpack_field_int(8).to_uint()));

  m_response_status = static_cast<uvm_tlm_response_status_e>(
    static_cast<std::int32_t>(packer.unpack_field_int(32).to_uint()));
  m_dmi = (packer.unpack_field_int(1).to_uint() != 0);

  m_byte_enable_length = packer.unpack_field_int(32).to_uint();
  m_byte_enable.clear();
  m_byte_enable.reserve(std::min<unsigned int>(m_byte_enable_length, 4096u));
  for (unsigned int i = 0; i < m_byte_enable_length; ++i)
    m_byte_enable.push_back(static_cast<unsigned char>(packer.unpack_field_int(8).to_uint()));

  m_streaming_width = packer.unpack_field_int(32).to_uint();
  m_gp_option = static_cast<uvm_tlm_gp_option_e>(
    static_cast<std::int32_t>(packer.unpack_field_int(32).to_uint()));
}

void uvm_tlm_generic_payload::do_record( const uvm_recorder& recorder )
{
  uvm_sequence_item::do_record(recorder);
  uvm_recorder* r = const_cast<uvm_recorder*>(&recorder);

  r->record_field_int("address", m_address, 64, UVM_HEX);
  r->record_string("command", m_command_string());
  r->record_field_int("data_length", m_length, 32, UVM_DEC);
  for (unsigned int i = 0; i < m_data.size(); ++i)
  {
    std::ostringstream idx;
    idx << "data[" << i << "]";
    r->record_field_int(idx.str(), m_data[i], 8, UVM_HEX);
  }
  r->record_string("response_status", get_response_string());
  r->record_field_int("dmi_allowed", m_dmi ? 1 : 0, 1, UVM_BIN);
  r->record_field_int("byte_enable_length", m_byte_enable_length, 32, UVM_DEC);
  for (unsigned int i = 0; i < m_byte_enable.size(); ++i)
  {
    std::ostringstream idx;
    idx << "byte_enable[" << i << "]";
    r->record_field_int(idx.str(), m_byte_enable[i], 8, UVM_HEX);
  }
  r->record_field_int("streaming_width", m_streaming_width, 32, UVM_DEC);
  r->record_string("gp_option", m_option_string());
}

std::string uvm_tlm_generic_payload::convert2string() const
{
  std::ostringstream os;
  os << m_command_string()
     << " addr=0x" << std::hex << m_address << std::dec
     << " len=" << m_length
     << " data=[";
  const std::size_t n = std::min<std::size_t>(m_data.size(), 16);
  for (std::size_t i = 0; i < n; ++i)
  {
    if (i > 0)
      os << " ";
    os << "0x" << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<unsigned int>(m_data[i]) << std::dec;
  }
  if (m_data.size() > n)
    os << " ...";
  os << "] resp=" << get_response_string();
  return os.str();
}

} // namespace uvm
