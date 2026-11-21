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

#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/print/uvm_printer_element.h"

namespace uvm {

uvm_printer_element::uvm_printer_element( const std::string& name )
: uvm_object(name),
  m_elem_name(""),
  m_elem_type_name(""),
  m_elem_size(""),
  m_elem_value(""),
  m_kind(ELEMENT_FIELD),
  m_parent(nullptr)
{}

uvm_printer_element::uvm_printer_element( const uvm_printer_element& rhs )
: uvm_object(rhs),
  m_elem_name(rhs.m_elem_name),
  m_elem_type_name(rhs.m_elem_type_name),
  m_elem_size(rhs.m_elem_size),
  m_elem_value(rhs.m_elem_value),
  m_kind(rhs.m_kind),
  m_parent(nullptr)
{
  m_children.reserve(rhs.m_children.size());
  for (std::size_t i = 0; i < rhs.m_children.size(); ++i)
  {
    if (rhs.m_children[i] != nullptr)
    {
      uvm_printer_element* clone = new uvm_printer_element(*rhs.m_children[i]);
      clone->m_parent = this;
      m_children.push_back(clone);
    }
  }
}

uvm_printer_element& uvm_printer_element::operator=( const uvm_printer_element& rhs )
{
  if (this == &rhs)
    return *this;

  clear_children();
  uvm_object::operator=(rhs);
  m_elem_name = rhs.m_elem_name;
  m_elem_type_name = rhs.m_elem_type_name;
  m_elem_size = rhs.m_elem_size;
  m_elem_value = rhs.m_elem_value;
  m_kind = rhs.m_kind;
  m_parent = nullptr;

  m_children.reserve(rhs.m_children.size());
  for (std::size_t i = 0; i < rhs.m_children.size(); ++i)
  {
    if (rhs.m_children[i] != nullptr)
    {
      uvm_printer_element* clone = new uvm_printer_element(*rhs.m_children[i]);
      clone->m_parent = this;
      m_children.push_back(clone);
    }
  }
  return *this;
}

uvm_printer_element::~uvm_printer_element()
{
  clear_children();
}

void uvm_printer_element::set( const std::string& element_name,
                               const std::string& element_type_name,
                               const std::string& element_size,
                               const std::string& element_value )
{
  m_elem_name = element_name;
  m_elem_type_name = element_type_name;
  m_elem_size = element_size;
  m_elem_value = element_value;
}

void uvm_printer_element::set_element_name( const std::string& element_name )
{
  m_elem_name = element_name;
}

std::string uvm_printer_element::get_element_name() const
{
  return m_elem_name;
}

void uvm_printer_element::set_element_type_name( const std::string& element_type_name )
{
  m_elem_type_name = element_type_name;
}

std::string uvm_printer_element::get_element_type_name() const
{
  return m_elem_type_name;
}

void uvm_printer_element::set_element_size( const std::string& element_size )
{
  m_elem_size = element_size;
}

std::string uvm_printer_element::get_element_size() const
{
  return m_elem_size;
}

void uvm_printer_element::set_element_value( const std::string& element_value )
{
  m_elem_value = element_value;
}

std::string uvm_printer_element::get_element_value() const
{
  return m_elem_value;
}

void uvm_printer_element::add_child( uvm_printer_element* child )
{
  if (child == nullptr)
    return;

  if (child == this)
  {
    UVM_WARNING("PRNTELEM", "Ignoring uvm_printer_element::add_child(this) self-cycle.");
    return;
  }

  for (const uvm_printer_element* anc = m_parent; anc != nullptr; anc = anc->m_parent)
  {
    if (anc == child)
    {
      UVM_WARNING("PRNTELEM", "Ignoring uvm_printer_element::add_child() ancestor cycle.");
      return;
    }
  }

  if (child->m_parent == this)
    return;

  if (child->m_parent != nullptr)
  {
    std::vector<uvm_printer_element*>& old_children = child->m_parent->m_children;
    old_children.erase(std::remove(old_children.begin(), old_children.end(), child),
                       old_children.end());
  }

  child->m_parent = this;
  m_children.push_back(child);
}

void uvm_printer_element::m_collect_children_recursive(
  std::vector<uvm_printer_element*>& children ) const
{
  for (std::size_t i = 0; i < m_children.size(); ++i)
  {
    uvm_printer_element* c = m_children[i];
    if (c != nullptr)
    {
      children.push_back(c);
      c->m_collect_children_recursive(children);
    }
  }
}

void uvm_printer_element::get_children( std::vector<uvm_printer_element*>& children,
                                        bool recurse ) const
{
  children.clear();
  if (!recurse)
  {
    children = m_children;
    return;
  }
  m_collect_children_recursive(children);
}

const std::vector<uvm_printer_element*>& uvm_printer_element::get_children() const
{
  return m_children;
}

void uvm_printer_element::clear_children()
{
  for (std::size_t i = 0; i < m_children.size(); ++i)
    delete m_children[i];
  m_children.clear();
}

bool uvm_printer_element::has_children() const
{
  return !m_children.empty();
}

uvm_printer_element* uvm_printer_element::get_parent() const
{
  return m_parent;
}

void uvm_printer_element::set_element_kind( element_kind_e kind )
{
  m_kind = kind;
}

uvm_printer_element::element_kind_e uvm_printer_element::get_element_kind() const
{
  if (m_kind == ELEMENT_FIELD && !m_children.empty())
    return ELEMENT_OBJECT;
  return m_kind;
}

uvm_printer_element_proxy::uvm_printer_element_proxy( const std::string& name )
: uvm_structure_proxy<uvm_printer_element>(name)
{}

uvm_printer_element_proxy::~uvm_printer_element_proxy()
{}

void uvm_printer_element_proxy::get_immediate_children(
  uvm_printer_element* s,
  std::vector<uvm_printer_element*>& children )
{
  if (s == nullptr)
    return;
  const std::vector<uvm_printer_element*>& direct = s->get_children();
  children.insert(children.end(), direct.begin(), direct.end());
}

} // namespace uvm
