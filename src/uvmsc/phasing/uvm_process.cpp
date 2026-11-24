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

#define SC_INCLUDE_DYNAMIC_PROCESSES
#include <systemc>

#include <map>
#include <memory>
#include <sstream>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/phasing/uvm_process.h"

namespace uvm {

namespace {

std::map<sc_core::sc_object*, std::unique_ptr<uvm_process> >& process_registry()
{
  static std::map<sc_core::sc_object*, std::unique_ptr<uvm_process> > registry;
  return registry;
}

} // namespace

uvm_process::uvm_process( const sc_core::sc_process_handle& handle )
: m_handle(handle),
  m_killed(false),
  m_rng()
{}

uvm_process* uvm_process::from_handle( const sc_core::sc_process_handle& handle )
{
  if (!handle.valid())
    return nullptr;

  sc_core::sc_object* key = handle.get_process_object();
  if (key == nullptr)
    return nullptr;

  std::map<sc_core::sc_object*, std::unique_ptr<uvm_process> >& reg = process_registry();
  std::map<sc_core::sc_object*, std::unique_ptr<uvm_process> >::iterator it = reg.find(key);
  if (it != reg.end())
    return it->second.get();

  std::unique_ptr<uvm_process> proc(new uvm_process(handle));
  uvm_process* raw = proc.get();
  reg.emplace(key, std::move(proc));
  return raw;
}

uvm_process* uvm_process::self()
{
  return from_handle(sc_core::sc_get_current_process_handle());
}

uvm_process::state uvm_process::status() const
{
  if (m_killed || m_handle.is_unwinding())
    return KILLED;

  if (!m_handle.valid() || m_handle.terminated())
    return FINISHED;

  sc_core::sc_process_b* proc =
    static_cast<sc_core::sc_process_b*>(m_handle.get_process_object());
  if (proc != nullptr)
  {
    const int st = proc->current_state();
    if ((st & sc_core::sc_process_b::ps_bit_zombie) != 0)
      return FINISHED;
    if ((st & sc_core::sc_process_b::ps_bit_suspended) != 0)
      return SUSPENDED;
  }

  if (m_handle == sc_core::sc_get_current_process_handle())
    return RUNNING;

  return WAITING;
}

void uvm_process::kill()
{
  if (!m_handle.valid() || m_handle.terminated() ||
      m_handle.is_unwinding() || !sc_core::sc_is_running())
    return;

  m_killed = true;
  m_handle.kill();
}

void uvm_process::await()
{
  if (!m_handle.valid() || m_handle.terminated())
    return;

  sc_core::sc_process_handle cur = sc_core::sc_get_current_process_handle();
  if (!cur.valid() || cur.proc_kind() == sc_core::SC_METHOD_PROC_)
  {
    uvm_report_error("UVM/PROC/AWAIT",
                     "Cannot call await() outside a SystemC thread process.",
                     UVM_NONE);
    return;
  }

  if (cur == m_handle)
    return;

  sc_core::wait(m_handle.terminated_event());
}

void uvm_process::suspend()
{
  if (m_handle.valid() && !m_handle.terminated() && !m_handle.is_unwinding())
    m_handle.suspend();
}

void uvm_process::resume()
{
  if (m_handle.valid() && !m_handle.terminated() && !m_handle.is_unwinding())
    m_handle.resume();
}

void uvm_process::srandom( unsigned int seed )
{
  m_rng.seed(seed);
}

std::string uvm_process::get_randstate() const
{
  std::ostringstream oss;
  oss << m_rng;
  return oss.str();
}

void uvm_process::set_randstate( const std::string& state_str )
{
  std::istringstream iss(state_str);
  std::mt19937 parsed;
  if (iss >> parsed)
    m_rng = parsed;
}

sc_core::sc_process_handle uvm_process::get_process_handle() const
{
  return m_handle;
}

std::mt19937& uvm_process::get_rng()
{
  return m_rng;
}

} // namespace uvm
