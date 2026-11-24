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

#include <sstream>
#include <string>
#include <vector>

#include "uvmsc/print/uvm_xml_printer.h"

namespace uvm {

namespace {

std::string escape_xml( const std::string& in )
{
  std::ostringstream oss;
  for (std::size_t i = 0; i < in.size(); ++i)
  {
    const unsigned char c = static_cast<unsigned char>(in[i]);
    switch (c)
    {
      case '&':  oss << "&amp;";  break;
      case '<':  oss << "&lt;";   break;
      case '>':  oss << "&gt;";   break;
      case '"':  oss << "&quot;"; break;
      case '\'': oss << "&apos;"; break;
      case '\t': oss << "&#x9;";  break;
      case '\n': oss << "&#xA;";  break;
      case '\r': oss << "&#xD;";  break;
      default:
        if (c < 0x20 || c == 0x7F)
          oss << '?';
        else
          oss << static_cast<char>(c);
        break;
    }
  }
  return oss.str();
}

void append_xml_indent( std::ostringstream& oss, int depth, int indent_width, const std::string& prefix )
{
  if (indent_width <= 0)
    return;
  oss << prefix;
  for (int i = 0; i < depth * indent_width; ++i)
    oss << ' ';
}

void emit_xml_element( const uvm_printer_element* elem,
                       const uvm_printer_knobs& knobs,
                       int depth,
                       std::ostringstream& oss )
{
  if (elem == nullptr)
    return;

  const uvm_printer_element::element_kind_e kind = elem->get_element_kind();
  const char* tag = "field";
  if (kind == uvm_printer_element::ELEMENT_OBJECT)
    tag = "object";
  else if (kind == uvm_printer_element::ELEMENT_ARRAY)
    tag = "array";
  else if (kind == uvm_printer_element::ELEMENT_ARRAY_RANGE)
    tag = "range";

  append_xml_indent(oss, depth, knobs.indent, knobs.prefix);
  oss << '<' << tag;

  if (knobs.identifier && kind != uvm_printer_element::ELEMENT_ARRAY_RANGE)
    oss << " name=\"" << escape_xml(elem->get_element_name()) << '"';

  const std::string typ = elem->get_element_type_name();
  if (knobs.type_name && !typ.empty() && kind != uvm_printer_element::ELEMENT_ARRAY_RANGE)
    oss << " type=\"" << escape_xml(typ) << '"';

  const std::string sz = elem->get_element_size();
  if (knobs.size && !sz.empty() && sz != "-" && kind != uvm_printer_element::ELEMENT_ARRAY_RANGE)
    oss << " size=\"" << escape_xml(sz) << '"';

  const std::string val = elem->get_element_value();
  if (kind == uvm_printer_element::ELEMENT_FIELD ||
      kind == uvm_printer_element::ELEMENT_ARRAY_RANGE ||
      (kind == uvm_printer_element::ELEMENT_OBJECT && knobs.reference && !val.empty() && val != "-"))
  {
    oss << " value=\"" << escape_xml(val) << '"';
  }

  const std::vector<uvm_printer_element*>& children = elem->get_children();
  if (children.empty())
  {
    oss << "/>";
    if (knobs.indent > 0) oss << '\n';
    return;
  }

  oss << '>';
  if (knobs.indent > 0) oss << '\n';

  for (std::size_t i = 0; i < children.size(); ++i)
    emit_xml_element(children[i], knobs, depth + 1, oss);

  append_xml_indent(oss, depth, knobs.indent, knobs.prefix);
  oss << "</" << tag << '>';
  if (knobs.indent > 0) oss << '\n';
}

} // namespace

uvm_xml_printer* uvm_default_xml_printer = nullptr;

uvm_xml_printer::uvm_xml_printer( const std::string& name )
: uvm_printer(name)
{
  knobs.header = true;
  knobs.footer = false;
  knobs.type_name = false;
  knobs.size = false;
  knobs.reference = false;
}

uvm_xml_printer::~uvm_xml_printer()
{}

uvm_xml_printer& uvm_xml_printer::m_builtin_default()
{
  static uvm_xml_printer builtin_default("uvm_default_xml_printer");
  return builtin_default;
}

uvm_xml_printer* uvm_xml_printer::get_default()
{
  if (uvm_default_xml_printer == nullptr)
    uvm_default_xml_printer = &m_builtin_default();
  return uvm_default_xml_printer;
}

void uvm_xml_printer::set_default( uvm_xml_printer* printer )
{
  uvm_default_xml_printer = (printer != nullptr) ? printer : &m_builtin_default();
}

std::string uvm_xml_printer::emit()
{
  std::ostringstream oss;

  if (knobs.header)
  {
    std::string hdr = format_header();
    if (hdr.empty())
      hdr = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    oss << knobs.prefix << hdr;
    if (knobs.indent > 0) oss << '\n';
  }

  const std::vector<uvm_printer_element*>& roots = get_top_elements();
  if (roots.empty())
  {
    oss << knobs.prefix << "<uvm_printer/>";
    if (knobs.indent > 0) oss << '\n';
  }
  else if (roots.size() == 1)
  {
    emit_xml_element(roots[0], knobs, 0, oss);
  }
  else
  {
    oss << knobs.prefix << "<uvm_printer>";
    if (knobs.indent > 0) oss << '\n';
    for (std::size_t i = 0; i < roots.size(); ++i)
      emit_xml_element(roots[i], knobs, 1, oss);
    oss << knobs.prefix << "</uvm_printer>";
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
