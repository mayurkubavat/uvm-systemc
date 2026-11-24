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

#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "uvmsc/print/uvm_json_printer.h"

namespace uvm {

namespace {

std::string escape_json( const std::string& in )
{
  std::ostringstream oss;
  for (std::size_t i = 0; i < in.size(); ++i)
  {
    const unsigned char c = static_cast<unsigned char>(in[i]);
    switch (c)
    {
      case '"':  oss << "\\\""; break;
      case '\\': oss << "\\\\"; break;
      case '\b': oss << "\\b";  break;
      case '\f': oss << "\\f";  break;
      case '\n': oss << "\\n";  break;
      case '\r': oss << "\\r";  break;
      case '\t': oss << "\\t";  break;
      default:
        if (c < 0x20)
        {
          oss << "\\u00"
              << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
              << static_cast<unsigned int>(c)
              << std::dec << std::nouppercase;
        }
        else
        {
          oss << static_cast<char>(c);
        }
        break;
    }
  }
  return oss.str();
}

void append_indent( std::ostringstream& oss, int depth, int indent_width, const std::string& prefix )
{
  if (indent_width <= 0)
    return;
  oss << '\n' << prefix;
  for (int i = 0; i < depth * indent_width; ++i)
    oss << ' ';
}

bool extract_assoc_key( const std::string& name, std::string& key_out )
{
  if (name.size() < 3 || name.front() != '[' || name.back() != ']')
    return false;
  const std::string inner = name.substr(1, name.size() - 2);
  bool all_digits = true;
  for (std::size_t i = 0; i < inner.size(); ++i)
  {
    if (!std::isdigit(static_cast<unsigned char>(inner[i])))
    {
      all_digits = false;
      break;
    }
  }
  if (all_digits)
    return false;
  key_out = inner;
  return true;
}

void emit_json_value( const uvm_printer_element* elem,
                      const uvm_printer_knobs& knobs,
                      int depth,
                      std::ostringstream& oss );

void emit_json_scalar( const uvm_printer_element* elem,
                       const uvm_printer_knobs& knobs,
                       int depth,
                       std::ostringstream& oss )
{
  const std::string val = elem->get_element_value();
  const std::string typ = elem->get_element_type_name();
  const std::string sz = elem->get_element_size();

  const bool include_type = knobs.type_name && !typ.empty();
  const bool include_size = knobs.size && !sz.empty() && sz != "-";
  if (!include_type && !include_size)
  {
    oss << '"' << escape_json(val) << '"';
    return;
  }

  oss << '{';
  bool first = true;
  if (include_type)
  {
    append_indent(oss, depth + 1, knobs.indent, knobs.prefix);
    oss << "\"type\":";
    if (knobs.indent > 0) oss << ' ';
    oss << '"' << escape_json(typ) << '"';
    first = false;
  }
  if (include_size)
  {
    if (!first) oss << ',';
    append_indent(oss, depth + 1, knobs.indent, knobs.prefix);
    oss << "\"size\":";
    if (knobs.indent > 0) oss << ' ';
    oss << '"' << escape_json(sz) << '"';
    first = false;
  }
  if (!first) oss << ',';
  append_indent(oss, depth + 1, knobs.indent, knobs.prefix);
  oss << "\"value\":";
  if (knobs.indent > 0) oss << ' ';
  oss << '"' << escape_json(val) << '"';
  append_indent(oss, depth, knobs.indent, knobs.prefix);
  oss << '}';
}

void emit_json_value( const uvm_printer_element* elem,
                      const uvm_printer_knobs& knobs,
                      int depth,
                      std::ostringstream& oss )
{
  if (elem == nullptr)
  {
    oss << "null";
    return;
  }

  const uvm_printer_element::element_kind_e kind = elem->get_element_kind();
  const std::vector<uvm_printer_element*>& children = elem->get_children();

  if (kind == uvm_printer_element::ELEMENT_ARRAY)
  {
    if (children.empty())
    {
      oss << "[]";
      return;
    }
    oss << '[';
    for (std::size_t i = 0; i < children.size(); ++i)
    {
      if (i > 0)
        oss << ',';
      append_indent(oss, depth + 1, knobs.indent, knobs.prefix);
      const uvm_printer_element* child = children[i];
      std::string assoc_key;
      if (child != nullptr && extract_assoc_key(child->get_element_name(), assoc_key))
      {
        oss << "{\"" << escape_json(assoc_key) << "\":";
        if (knobs.indent > 0) oss << ' ';
        emit_json_value(child, knobs, depth + 1, oss);
        oss << '}';
      }
      else
      {
        emit_json_value(child, knobs, depth + 1, oss);
      }
    }
    append_indent(oss, depth, knobs.indent, knobs.prefix);
    oss << ']';
    return;
  }

  if (kind == uvm_printer_element::ELEMENT_OBJECT)
  {
    const bool include_type = knobs.type_name && !elem->get_element_type_name().empty();
    const bool include_ref = knobs.reference &&
                             !elem->get_element_value().empty() &&
                             elem->get_element_value() != "-";
    if (!include_type && !include_ref && children.empty())
    {
      oss << "{}";
      return;
    }

    oss << '{';
    bool first = true;
    if (include_type)
    {
      append_indent(oss, depth + 1, knobs.indent, knobs.prefix);
      oss << "\"__type__\":";
      if (knobs.indent > 0) oss << ' ';
      oss << '"' << escape_json(elem->get_element_type_name()) << '"';
      first = false;
    }
    if (include_ref)
    {
      if (!first) oss << ',';
      append_indent(oss, depth + 1, knobs.indent, knobs.prefix);
      oss << "\"__ref__\":";
      if (knobs.indent > 0) oss << ' ';
      oss << '"' << escape_json(elem->get_element_value()) << '"';
      first = false;
    }
    for (std::size_t i = 0; i < children.size(); ++i)
    {
      const uvm_printer_element* child = children[i];
      if (child == nullptr)
        continue;
      if (!first) oss << ',';
      append_indent(oss, depth + 1, knobs.indent, knobs.prefix);
      std::string key = child->get_element_name();
      if (key.empty())
        key = "unnamed";
      oss << '"' << escape_json(key) << "\":";
      if (knobs.indent > 0) oss << ' ';
      emit_json_value(child, knobs, depth + 1, oss);
      first = false;
    }
    append_indent(oss, depth, knobs.indent, knobs.prefix);
    oss << '}';
    return;
  }

  emit_json_scalar(elem, knobs, depth, oss);
}

} // namespace

uvm_json_printer* uvm_default_json_printer = nullptr;

uvm_json_printer::uvm_json_printer( const std::string& name )
: uvm_printer(name)
{
  knobs.header = false;
  knobs.footer = false;
  knobs.type_name = false;
  knobs.size = false;
  knobs.reference = false;
}

uvm_json_printer::~uvm_json_printer()
{}

uvm_json_printer& uvm_json_printer::m_builtin_default()
{
  static uvm_json_printer builtin_default("uvm_default_json_printer");
  return builtin_default;
}

uvm_json_printer* uvm_json_printer::get_default()
{
  if (uvm_default_json_printer == nullptr)
    uvm_default_json_printer = &m_builtin_default();
  return uvm_default_json_printer;
}

void uvm_json_printer::set_default( uvm_json_printer* printer )
{
  uvm_default_json_printer = (printer != nullptr) ? printer : &m_builtin_default();
}

std::string uvm_json_printer::emit()
{
  std::ostringstream oss;

  if (knobs.header)
  {
    const std::string hdr = format_header();
    if (!hdr.empty())
    {
      oss << knobs.prefix << hdr;
      if (knobs.indent > 0) oss << '\n';
    }
  }

  const std::vector<uvm_printer_element*>& roots = get_top_elements();
  if (roots.empty())
  {
    oss << knobs.prefix << "{}";
    if (knobs.indent > 0) oss << '\n';
  }
  else if (roots.size() == 1)
  {
    const uvm_printer_element* root = roots[0];
    const std::string root_name = (root != nullptr) ? root->get_element_name() : "";
    if (knobs.identifier && !root_name.empty())
    {
      oss << knobs.prefix << '{';
      append_indent(oss, 1, knobs.indent, knobs.prefix);
      oss << '"' << escape_json(root_name) << "\":";
      if (knobs.indent > 0) oss << ' ';
      emit_json_value(root, knobs, 1, oss);
      append_indent(oss, 0, knobs.indent, knobs.prefix);
      oss << '}';
      if (knobs.indent > 0) oss << '\n';
    }
    else
    {
      oss << knobs.prefix;
      emit_json_value(root, knobs, 0, oss);
      if (knobs.indent > 0) oss << '\n';
    }
  }
  else
  {
    oss << knobs.prefix << '[';
    for (std::size_t i = 0; i < roots.size(); ++i)
    {
      if (i > 0) oss << ',';
      append_indent(oss, 1, knobs.indent, knobs.prefix);
      const uvm_printer_element* root = roots[i];
      const std::string root_name = (root != nullptr) ? root->get_element_name() : "";
      if (knobs.identifier && !root_name.empty())
      {
        oss << '{';
        append_indent(oss, 2, knobs.indent, knobs.prefix);
        oss << '"' << escape_json(root_name) << "\":";
        if (knobs.indent > 0) oss << ' ';
        emit_json_value(root, knobs, 2, oss);
        append_indent(oss, 1, knobs.indent, knobs.prefix);
        oss << '}';
      }
      else
      {
        emit_json_value(root, knobs, 1, oss);
      }
    }
    append_indent(oss, 0, knobs.indent, knobs.prefix);
    oss << ']';
    if (knobs.indent > 0) oss << '\n';
  }

  if (knobs.footer)
  {
    const std::string ftr = format_footer();
    if (!ftr.empty())
    {
      oss << knobs.prefix << ftr;
      if (knobs.indent > 0) oss << '\n';
    }
  }

  flush();
  return oss.str();
}

} // namespace uvm
