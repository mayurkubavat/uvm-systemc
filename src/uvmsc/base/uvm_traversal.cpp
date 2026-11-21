//----------------------------------------------------------------------
//   Copyright 2010-2018 Cadence Design Systems, Inc.
//   Copyright 2014-2024 NVIDIA Corporation
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

#include "uvmsc/base/uvm_traversal.h"
#include "uvmsc/base/uvm_component.h"
#include "uvmsc/base/uvm_coreservice_t.h"
#include "uvmsc/base/uvm_default_coreservice_t.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/base/uvm_root.h"
#include "uvmsc/macros/uvm_message_defines.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_component_proxy
//----------------------------------------------------------------------

uvm_component_proxy::uvm_component_proxy( const std::string& name )
: uvm_structure_proxy<uvm_component>(name)
{}

//----------------------------------------------------------------------
// member function: get_immediate_children (virtual)
//
//! Appends the children of component s, as returned by
//! uvm_component::get_children, to the vector children.
//----------------------------------------------------------------------

void uvm_component_proxy::get_immediate_children(
  uvm_component* s,
  std::vector<uvm_component*>& children )
{
  s->get_children(children);
}

const std::string uvm_component_proxy::get_type_name() const
{
  return "uvm::uvm_component_proxy";
}

uvm_component_proxy::~uvm_component_proxy()
{}

//----------------------------------------------------------------------
// CLASS: uvm_component_name_check_visitor
//----------------------------------------------------------------------

uvm_component_name_check_visitor::uvm_component_name_check_visitor(
  const std::string& name )
: uvm_visitor<uvm_component>(name), m_root(nullptr)
{}

//----------------------------------------------------------------------
// member function: get_name_constraint (virtual)
//
//! Returns the regular expression which the name of each component
//! shall match.
//----------------------------------------------------------------------

std::string uvm_component_name_check_visitor::get_name_constraint() const
{
  return "/^[][[:alnum:](){}_:-]([][[:alnum:](){} _:-]*[][[:alnum:](){}_:-])?$/";
}

//----------------------------------------------------------------------
// member function: visit (virtual)
//
//! Issues the warning UVM/COMP/NAME if the name of the component node
//! does not match the regular expression returned by
//! get_name_constraint. The name of uvm_root is not checked.
//----------------------------------------------------------------------

void uvm_component_name_check_visitor::visit( uvm_component* node )
{
  if (node == nullptr || node == m_root)
    return;

  std::string regex = get_name_constraint();

  if ( (regex.length() <= 2) ||
       (regex[0] != '/') ||
       (regex[regex.length()-1] != '/') )
    regex = "/" + regex + "/";

  if ( !uvm_is_match(regex, node->get_name()) )
    UVM_WARNING("UVM/COMP/NAME", "the name \"" + node->get_name() +
      "\" of the component \"" + node->get_full_name() +
      "\" violates the uvm component name constraints");
}

//----------------------------------------------------------------------
// member function: begin_v (virtual)
//
//! Retrieves the uvm_root instance, which is excluded from the check.
//----------------------------------------------------------------------

void uvm_component_name_check_visitor::begin_v()
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  m_root = cs->get_root();
}

const std::string uvm_component_name_check_visitor::get_type_name() const
{
  return "uvm::uvm_component_name_check_visitor";
}

uvm_component_name_check_visitor::~uvm_component_name_check_visitor()
{}

} // namespace uvm
