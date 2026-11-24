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
#include <cmath>
#include <limits>
#include <sstream>

#include "uvmsc/tlm2/uvm_tlm_time.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/macros/uvm_message_defines.h"

namespace uvm {

double& uvm_tlm_time::m_default_res()
{
  static double s_default_res = 1.0e-12;
  return s_default_res;
}

void uvm_tlm_time::set_time_resolution( double res )
{
  if (!std::isfinite(res) || res <= 0.0)
  {
    UVM_ERROR("UVM/TLM/BADRES",
              "uvm_tlm_time::set_time_resolution requires a finite positive resolution.");
    return;
  }
  m_default_res() = res;
}

double uvm_tlm_time::get_time_resolution()
{
  return m_default_res();
}

uvm_tlm_time::uvm_tlm_time( const std::string& name, double res )
  : m_name(name),
    m_res(m_default_res()),
    m_time(0.0)
{
  if (!std::isfinite(res) || res <= 0.0)
  {
    UVM_ERROR("UVM/TLM/BADRES",
              "uvm_tlm_time constructed with zero or negative resolution; using default resolution.");
    m_res = m_default_res();
  }
  else
  {
    m_res = res;
  }
}

std::string uvm_tlm_time::get_name() const
{
  return m_name;
}

void uvm_tlm_time::reset()
{
  m_time = 0.0;
}

void uvm_tlm_time::incr( double t, double scaled, double secs )
{
  if (!std::isfinite(t) || t < 0.0)
  {
    UVM_ERROR("UVM/TLM/NEGTIME",
              "uvm_tlm_time::incr cannot increment by a negative or non-finite time.");
    return;
  }
  if (!std::isfinite(scaled) || scaled <= 0.0 || !std::isfinite(secs) || secs <= 0.0)
  {
    UVM_ERROR("UVM/TLM/BADSCALE",
              "uvm_tlm_time::incr requires finite positive scaling factors.");
    return;
  }
  const double delta_sec = (t * scaled) / secs;
  m_time += delta_sec / m_res;
}

void uvm_tlm_time::incr( const sc_core::sc_time& t )
{
  const double sec = t.to_seconds();
  if (!std::isfinite(sec) || sec < 0.0)
  {
    UVM_ERROR("UVM/TLM/NEGTIME",
              "uvm_tlm_time::incr cannot increment by a negative sc_time.");
    return;
  }
  m_time += sec / m_res;
}

void uvm_tlm_time::decr( double t, double scaled, double secs )
{
  if (!std::isfinite(t) || t < 0.0)
  {
    UVM_ERROR("UVM/TLM/NEGTIME",
              "uvm_tlm_time::decr cannot decrement by a negative or non-finite time.");
    return;
  }
  if (!std::isfinite(scaled) || scaled <= 0.0 || !std::isfinite(secs) || secs <= 0.0)
  {
    UVM_ERROR("UVM/TLM/BADSCALE",
              "uvm_tlm_time::decr requires finite positive scaling factors.");
    return;
  }
  const double delta_units = ((t * scaled) / secs) / m_res;
  const double tol = 16.0 * std::numeric_limits<double>::epsilon() * std::min(m_time, delta_units);
  if (delta_units > m_time + tol)
  {
    UVM_ERROR("UVM/TLM/NEGTIME",
              "uvm_tlm_time::decr would result in negative time; clamping to zero.");
    m_time = 0.0;
    return;
  }
  m_time = std::max(0.0, m_time - delta_units);
}

void uvm_tlm_time::decr( const sc_core::sc_time& t )
{
  const double sec = t.to_seconds();
  if (!std::isfinite(sec) || sec < 0.0)
  {
    UVM_ERROR("UVM/TLM/NEGTIME",
              "uvm_tlm_time::decr cannot decrement by a negative sc_time.");
    return;
  }
  const double delta_units = sec / m_res;
  const double tol = 16.0 * std::numeric_limits<double>::epsilon() * std::min(m_time, delta_units);
  if (delta_units > m_time + tol)
  {
    UVM_ERROR("UVM/TLM/NEGTIME",
              "uvm_tlm_time::decr would result in negative time; clamping to zero.");
    m_time = 0.0;
    return;
  }
  m_time = std::max(0.0, m_time - delta_units);
}

double uvm_tlm_time::get_realtime( double scaled, double secs ) const
{
  if (!std::isfinite(scaled) || scaled <= 0.0 || !std::isfinite(secs) || secs <= 0.0)
  {
    UVM_ERROR("UVM/TLM/BADSCALE",
              "uvm_tlm_time::get_realtime requires finite positive scaling factors.");
    return 0.0;
  }
  return (m_time * m_res * secs) / scaled;
}

double uvm_tlm_time::get_abstime( double scaled ) const
{
  if (!std::isfinite(scaled) || scaled <= 0.0)
  {
    UVM_ERROR("UVM/TLM/BADSCALE",
              "uvm_tlm_time::get_abstime requires a finite positive scaling factor.");
    return 0.0;
  }
  return (m_time * m_res) / scaled;
}

void uvm_tlm_time::set_abstime( double t, double scaled )
{
  if (!std::isfinite(t) || t < 0.0)
  {
    UVM_ERROR("UVM/TLM/NEGTIME",
              "uvm_tlm_time::set_abstime cannot set a negative or non-finite time.");
    return;
  }
  if (!std::isfinite(scaled) || scaled <= 0.0)
  {
    UVM_ERROR("UVM/TLM/BADSCALE",
              "uvm_tlm_time::set_abstime requires a finite positive scaling factor.");
    return;
  }
  m_time = (t * scaled) / m_res;
}

sc_core::sc_time uvm_tlm_time::to_sc_time() const
{
  return sc_core::sc_time::from_seconds(m_time * m_res);
}

void uvm_tlm_time::from_sc_time( const sc_core::sc_time& t )
{
  const double sec = t.to_seconds();
  m_time = (sec > 0.0) ? (sec / m_res) : 0.0;
}

double uvm_tlm_time::get_resolution() const
{
  return m_res;
}

std::string uvm_tlm_time::to_string() const
{
  std::ostringstream os;
  os << m_name << ": " << (m_time * m_res) << " s (res=" << m_res << " s)";
  return os.str();
}

bool uvm_tlm_time::operator==( const uvm_tlm_time& rhs ) const
{
  const double min_res = std::min(m_res, rhs.m_res);
  const double lhs_units = (m_time * m_res) / min_res;
  const double rhs_units = (rhs.m_time * rhs.m_res) / min_res;
  if (std::isfinite(lhs_units) && std::isfinite(rhs_units))
  {
    const double tol = std::max(
      1.0e-12,
      16.0 * std::numeric_limits<double>::epsilon() * std::max(lhs_units, rhs_units));
    return std::fabs(lhs_units - rhs_units) <= tol;
  }
  const double lhs_sec = m_time * m_res;
  const double rhs_sec = rhs.m_time * rhs.m_res;
  const double tol_sec =
    16.0 * std::numeric_limits<double>::epsilon() * std::max(lhs_sec, rhs_sec);
  return std::fabs(lhs_sec - rhs_sec) <= tol_sec;
}

bool uvm_tlm_time::operator!=( const uvm_tlm_time& rhs ) const
{
  return !(*this == rhs);
}

} // namespace uvm
