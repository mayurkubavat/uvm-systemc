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

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/factory/uvm_factory.h"
#include "uvmsc/policy/uvm_policy_base.h"

namespace uvm {

uvm_policy::uvm_policy( const std::string& name )
: uvm_object(name)
{}

uvm_policy::uvm_policy( const std::string& name, no_inst_count_t )
: uvm_object(name, no_inst_count_t())
{}

uvm_policy::~uvm_policy()
{
  m_policy_stack.clear();
  m_policy_extensions.clear();
}

void uvm_policy::push_active_object( uvm_object* obj )
{
  if (obj == nullptr)
    return;

  m_policy_stack.push_back(obj);
}

uvm_object* uvm_policy::pop_active_object()
{
  if (m_policy_stack.empty())
    return nullptr;

  uvm_object* top = m_policy_stack.back();
  m_policy_stack.pop_back();
  return top;
}

uvm_object* uvm_policy::get_active_object() const
{
  if (m_policy_stack.empty())
    return nullptr;

  return m_policy_stack.back();
}

unsigned int uvm_policy::get_active_object_depth() const
{
  return static_cast<unsigned int>(m_policy_stack.size());
}

bool uvm_policy::is_active_object( const uvm_object* obj ) const
{
  if (obj == nullptr)
    return false;

  return std::find(m_policy_stack.begin(), m_policy_stack.end(), obj) != m_policy_stack.end();
}

void uvm_policy::flush()
{
  m_policy_stack.clear();
}

void uvm_policy::set_extension( uvm_object* extension )
{
  if (extension == nullptr)
    return;

  const uvm_object_wrapper* ext_type = extension->get_object_type();
  if (ext_type == nullptr)
  {
    uvm_report_error("UVM/POLICY/NO_EXT_TYPE",
                     "uvm_policy::set_extension() failed: extension has null object type",
                     UVM_NONE);
    return;
  }

  m_policy_extensions[ext_type] = extension;
}

bool uvm_policy::extension_exists( const uvm_object_wrapper* extension_type ) const
{
  if (extension_type == nullptr)
    return false;

  return m_policy_extensions.find(extension_type) != m_policy_extensions.end();
}

uvm_object* uvm_policy::get_extension( const uvm_object_wrapper* extension_type ) const
{
  if (extension_type == nullptr)
    return nullptr;

  std::map<const uvm_object_wrapper*, uvm_object*>::const_iterator it =
    m_policy_extensions.find(extension_type);
  if (it == m_policy_extensions.end())
    return nullptr;

  return it->second;
}

void uvm_policy::clear_extension( const uvm_object_wrapper* extension_type )
{
  if (extension_type == nullptr)
    return;

  m_policy_extensions.erase(extension_type);
}

void uvm_policy::clear_extensions()
{
  m_policy_extensions.clear();
}

} // namespace uvm
