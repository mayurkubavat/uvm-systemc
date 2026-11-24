//----------------------------------------------------------------------
//   Copyright 2019 COSEDA Technologies GmbH
//   Copyright 2014 Fraunhofer-Gesellschaft zur Foerderung
//					der angewandten Forschung e.V.
//   Copyright 2012-2021 NXP B.V.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2011 Cadence Design Systems, Inc.
//   Copyright 2010 Synopsys, Inc.
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

#include <map>
#include <sstream>

#include "uvmsc/base/uvm_component.h"
#include "uvmsc/base/uvm_object.h"
#include "uvmsc/print/uvm_printer.h"
#include "uvmsc/print/uvm_printer_globals.h"
#include "uvmsc/misc/uvm_status_container.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/misc/uvm_misc.h"
#include "uvmsc/misc/uvm_scope_stack.h"

namespace uvm {

namespace {

class active_object_guard
{
 public:
  active_object_guard( uvm_policy* policy, uvm_object* obj )
  : m_policy(policy)
  {
    if (m_policy != nullptr)
      m_policy->push_active_object(obj);
  }

  ~active_object_guard()
  {
    if (m_policy != nullptr)
      m_policy->pop_active_object();
  }

 private:
  active_object_guard( const active_object_guard& );
  active_object_guard& operator=( const active_object_guard& );

  uvm_policy* m_policy;
};

class printer_cycle_guard
{
 public:
  explicit printer_cycle_guard( const uvm_object* obj )
  : m_obj(obj)
  {
    if (m_obj != nullptr && uvm_object::__m_uvm_status_container != nullptr)
      uvm_object::__m_uvm_status_container->cycle_check[m_obj] = true;
  }

  ~printer_cycle_guard()
  {
    if (m_obj != nullptr && uvm_object::__m_uvm_status_container != nullptr)
      uvm_object::__m_uvm_status_container->cycle_check.erase(m_obj);
  }

 private:
  printer_cycle_guard( const printer_cycle_guard& );
  printer_cycle_guard& operator=( const printer_cycle_guard& );

  const uvm_object* m_obj;
};

class printer_scope_guard
{
 public:
  printer_scope_guard( uvm_scope_stack* scope,
                       const std::string& name,
                       const char* up_sep )
  : m_scope(scope), m_up_sep(up_sep)
  {
    if (m_scope != nullptr)
      m_scope->down(name);
  }

  ~printer_scope_guard()
  {
    if (m_scope != nullptr)
      m_scope->up(m_up_sep);
  }

 private:
  printer_scope_guard( const printer_scope_guard& );
  printer_scope_guard& operator=( const printer_scope_guard& );

  uvm_scope_stack* m_scope;
  const char* m_up_sep;
};

class element_pop_guard
{
 public:
  explicit element_pop_guard( const uvm_printer* printer )
  : m_printer(printer)
  {}

  ~element_pop_guard()
  {
    if (m_printer != nullptr)
      m_printer->pop_element();
  }

 private:
  element_pop_guard( const element_pop_guard& );
  element_pop_guard& operator=( const element_pop_guard& );

  const uvm_printer* m_printer;
};

class print_object_rollback_guard
{
 public:
  print_object_rollback_guard( std::vector<uvm_printer_element*>& elem_stack,
                               std::list<bool>& array_stack,
                               uvm_scope_stack& scope )
  : m_elem_stack(elem_stack),
    m_array_stack(array_stack),
    m_scope(scope),
    m_elem_depth(elem_stack.size()),
    m_array_depth(array_stack.size()),
    m_scope_depth(scope.m_stack.size())
  {}

  ~print_object_rollback_guard()
  {
    while (m_elem_stack.size() > m_elem_depth)
      m_elem_stack.pop_back();
    while (m_array_stack.size() > m_array_depth)
      m_array_stack.pop_back();
    while (m_scope.m_stack.size() > m_scope_depth)
      m_scope.m_stack.pop_back();
    m_scope.m_arg.clear();
  }

 private:
  print_object_rollback_guard( const print_object_rollback_guard& );
  print_object_rollback_guard& operator=( const print_object_rollback_guard& );

  std::vector<uvm_printer_element*>& m_elem_stack;
  std::list<bool>& m_array_stack;
  uvm_scope_stack& m_scope;
  std::size_t m_elem_depth;
  std::size_t m_array_depth;
  std::size_t m_scope_depth;
};

void record_clone_map( const uvm_printer_element* src,
                       uvm_printer_element* dst,
                       std::map<const uvm_printer_element*, uvm_printer_element*>& ptr_map )
{
  if (src == nullptr || dst == nullptr)
    return;
  ptr_map[src] = dst;
  const std::vector<uvm_printer_element*>& sc = src->get_children();
  const std::vector<uvm_printer_element*>& dc = dst->get_children();
  const std::size_t count = (sc.size() < dc.size()) ? sc.size() : dc.size();
  for (std::size_t i = 0; i < count; ++i)
    record_clone_map(sc[i], dc[i], ptr_map);
}

} // namespace

//----------------------------------------------------------------------
// Class implementation: uvm_printer
//----------------------------------------------------------------------


//----------------------------------------------------------------------
// member function: print_field
//
//! Prints an integral field (of maximum default size of 4096 bits)
//!
//! \p name  - The name of the field.
//! \p value - The value of the field.
//! \p size  - The number of bits of the field (maximum is 4096).
//! \p radix - The radix to use for printing. The printer knob for radix is used
//!           if no radix is specified.
//! \p scope_separator - is used to find the leaf name since many printers only
//!           print the leaf name of a field.  Typical values for the separator
//!           are . (dot) or [ (open bracket).
//----------------------------------------------------------------------

void uvm_printer::print_field( const std::string& name,
                               const uvm_bitstream_t& value,
                               int size,
                               uvm_radix_enum radix,
                               const char* scope_separator,
                               const std::string& type_name ) const
{
  uvm_printer_row_info row_info;
  std::string sz_str, val_str;
  std::string loc_type_name = type_name;

  if(!name.empty())
  {
    m_scope.set_arg(name);
    //name = m_scope.get(); // not possible now to return name
  }

  if(loc_type_name.empty())
  {
    if(radix == UVM_TIME)
      loc_type_name ="time";
    else if(radix == UVM_STRING)
      loc_type_name ="string";
    else
      loc_type_name ="int";
  }

  if (size == -1)
    size = value.length();

  std::ostringstream str;
  str << size;
  sz_str = str.str(); //itoa

  if(radix == UVM_NORADIX)
    radix = knobs.default_radix;

  val_str = uvm_vector_to_string( value, size, radix,
                                  knobs.get_radix_str(radix));

  row_info.level = m_scope.depth();
  row_info.name = adjust_name(m_scope.get(), scope_separator);
  row_info.type_name = loc_type_name;
  row_info.size = sz_str;
  row_info.val = val_str;

  m_rows.push_back(row_info);
  m_push_element(row_info.name, row_info.type_name, row_info.size, row_info.val,
                 uvm_printer_element::ELEMENT_FIELD);
  pop_element();
}

//----------------------------------------------------------------------
// member function: print_field_int
//
//! Prints an integral field (up to 64 bits)
//!
//! \p name  - The name of the field.
//! \p value - The value of the field.
//! \p size  - The number of bits of the field (maximum is 64).
//! \p radix - The radix to use for printing. The printer knob for radix is used
//!           if no radix is specified.
//! \p scope_separator - is used to find the leaf name since many printers only
//!           print the leaf name of a field.  Typical values for the separator
//!           are . (dot) or [ (open bracket).
//----------------------------------------------------------------------

void uvm_printer::print_field_int( const std::string& name,
                                   const uvm_integral_t& value,
                                   int size,
                                   uvm_radix_enum radix,
                                   const char* scope_separator,
                                   const std::string& type_name ) const
{
  uvm_printer_row_info row_info;
  std::string sz_str, val_str;
  std::string loc_type_name = type_name;

  if(!name.empty())
  {
    m_scope.set_arg(name);
    //name = m_scope.get(); // TODO - not possible now to return name
  }

  if(loc_type_name.empty())
  {
    if(radix == UVM_TIME)
      loc_type_name ="time";
    else if(radix == UVM_STRING)
      loc_type_name ="string";
    else
      loc_type_name ="int";
  }

  if (size == -1)
    size = value.length();

  std::ostringstream str;
  str << size;
  sz_str = str.str(); //itoa

  if(radix == UVM_NORADIX)
    radix = knobs.default_radix;

  val_str = uvm_vector_to_string( value, size, radix,
                                  knobs.get_radix_str(radix));

  row_info.level = m_scope.depth();
  row_info.name = adjust_name(m_scope.get(), scope_separator);
  row_info.type_name = loc_type_name;
  row_info.size = sz_str;
  row_info.val = val_str;

  m_rows.push_back(row_info);
  m_push_element(row_info.name, row_info.type_name, row_info.size, row_info.val,
                 uvm_printer_element::ELEMENT_FIELD);
  pop_element();
}


//----------------------------------------------------------------------
// member function: print_real
//
//! Prints a real-value field.
//----------------------------------------------------------------------

void uvm_printer::print_real( const std::string& name,
                              double value,
                              const char* scope_separator ) const
{
  uvm_printer_row_info row_info;

  if (!name.empty() && name != "...")
  {
    m_scope.set_arg(name);
    //name = m_scope.get(); // no need to return changed name
  }

  std::ostringstream str_size;
  str_size << sizeof(value)*CHAR_BIT;

  std::ostringstream str_val;
  str_val << value;

  row_info.level = m_scope.depth();
  row_info.name = adjust_name(m_scope.get(), scope_separator);
  row_info.type_name = "double";
  row_info.size = str_size.str();
  row_info.val = str_val.str();

  m_rows.push_back(row_info);
  m_push_element(row_info.name, row_info.type_name, row_info.size, row_info.val,
                 uvm_printer_element::ELEMENT_FIELD);
  pop_element();
}

//----------------------------------------------------------------------
// member function: print_double
//
//! Prints a real-value field.
//----------------------------------------------------------------------

void uvm_printer::print_double( const std::string& name,
                              double value,
                              const char* scope_separator ) const

{
  print_real(name, value, scope_separator);
}

//----------------------------------------------------------------------
// member function: print_object
//
//! Prints the object
//----------------------------------------------------------------------

void uvm_printer::print_object( const std::string& name,
                                const uvm_object& obj,
                                const char* scope_separator ) const
{
  const uvm_component* comp = nullptr;
  const uvm_component* child_comp = nullptr;

  const uvm_object* objp = &obj;

  print_object_rollback_guard rollback_guard(m_element_stack, m_array_stack, m_scope);
  print_object_header(name, obj, scope_separator);
  element_pop_guard elem_guard(this);

  if( (knobs.depth == -1 || (knobs.depth > m_scope.depth())) &&
        (objp->__m_uvm_status_container->cycle_check.find(objp) == objp->__m_uvm_status_container->cycle_check.end()) // not exists
    )
  {
    printer_cycle_guard cyc_guard(objp);
    active_object_guard act_guard(const_cast<uvm_printer*>(this),
                                  const_cast<uvm_object*>(objp));
    const std::string scope_name = (name.empty() && objp != nullptr) ? objp->get_name() : name;
    const char* up_sep = (!name.empty() && name[0] == '[') ? "[" : ".";
    printer_scope_guard sc_guard(&m_scope, scope_name, up_sep);

    // Handle children of the comp
    comp = dynamic_cast<const uvm_component*>(objp);
    if(comp != nullptr)
    {
      std::string lname;
      if (comp->get_first_child(lname))
        do
        {
          child_comp = comp->get_child(lname);
          if(child_comp != nullptr && child_comp->print_enabled)
            this->print_object("", *child_comp);
        }
        while (comp->get_next_child(lname));
    }

    // print members of object
    objp->sprint(const_cast<uvm_printer*>(this));
  }
}

//----------------------------------------------------------------------
// member function: print_object_header
//
//! Print the object header
//----------------------------------------------------------------------

void uvm_printer::print_object_header( const std::string& name,
                                       const uvm_object& obj,
                                       const char* scope_separator ) const
{
  uvm_printer_row_info row_info;
  const uvm_component* comp;
  std::string lname = name;

  const uvm_object* objp = &obj;

  if(lname.empty())
  {
    if(objp != nullptr )
    {
      comp = dynamic_cast<const uvm_component*>(objp);
      if( (m_scope.depth() == 0) && (comp != nullptr) )
        lname = comp->get_full_name();
      else
        lname = objp->get_name();
    }
  }

  if(lname.empty())
    lname = "<unnamed>";

  m_scope.set_arg(lname);
  row_info.level = m_scope.depth();

  if(row_info.level == 0 && knobs.show_root == true)
    row_info.name = objp->get_full_name();
  else
    row_info.name = adjust_name(m_scope.get(), scope_separator);

  row_info.type_name = (objp != nullptr) ?  objp->get_type_name() : "object";
  row_info.size = "-";
  row_info.val = knobs.reference ? uvm_object_value_str(objp) : "-";

  m_rows.push_back(row_info);
  m_push_element(row_info.name, row_info.type_name, row_info.size, row_info.val,
                 uvm_printer_element::ELEMENT_OBJECT);
}

//----------------------------------------------------------------------
// member function: print_string
//
//! Prints a string field.
//----------------------------------------------------------------------

void uvm_printer::print_string( const std::string& name,
                                const std::string& value,
                                const char* scope_separator ) const
{
  uvm_printer_row_info row_info;
  std::string lname = name;

  if(!name.empty())
    m_scope.set_arg(name);

  row_info.level = m_scope.depth();
  row_info.name = adjust_name(m_scope.get(),scope_separator);
  row_info.type_name = "string";

  std::ostringstream str;
  str << value.length();

  row_info.size = str.str();
  row_info.val = (value.empty() ? "\"\"" : value);

  m_rows.push_back(row_info);
  m_push_element(row_info.name, row_info.type_name, row_info.size, value,
                 uvm_printer_element::ELEMENT_FIELD);
  pop_element();
}

//----------------------------------------------------------------------
// member function: print_time
//
//! Prints a time value. name is the name of the field, and value is the
//! value to print.
//----------------------------------------------------------------------

void uvm_printer::print_time( const std::string& name,
                              const sc_core::sc_time& value,
                              const char* scope_separator ) const
{
  print_generic(name, "time", 64, value.to_string(), scope_separator);
}

//----------------------------------------------------------------------
// member function: print_generic
//
//! Prints a field having the given \p name, \p type_name, \p size, and \p value.
//----------------------------------------------------------------------

void uvm_printer::print_generic( const std::string& name,
                                 const std::string& type_name,
                                 int size,
                                 const std::string& value,
                                 const char* scope_separator ) const
{
  uvm_printer_row_info row_info;
  std::string lname = name;

  if (!name.empty() && name != "...")
  {
    m_scope.set_arg(name);
    lname = m_scope.get();
  }

  std::ostringstream str;
  str << size;
  row_info.level = m_scope.depth();
  row_info.name = adjust_name(lname, scope_separator);
  row_info.type_name = type_name;
  row_info.size = (size == -2 ? "..." : str.str());
  row_info.val = (value.empty() ? "\"\"" : value);

  m_rows.push_back(row_info);
  const uvm_printer_element::element_kind_e kind =
    (name == "..." && size == -2) ? uvm_printer_element::ELEMENT_ARRAY_RANGE
                                  : uvm_printer_element::ELEMENT_FIELD;
  m_push_element(row_info.name, row_info.type_name, row_info.size, value, kind);
  pop_element();
}

//----------------------------------------------------------------------
// member function: emit
//
//! Emits a string representing the contents of an object
//! in a format defined by an extension of this object.
//----------------------------------------------------------------------

std::string uvm_printer::emit()
{
  uvm_report_error("NO_OVERRIDE","emit() method not overridden in printer subtype");
  return "";
}

//----------------------------------------------------------------------
// member function: format_row
//
//! Hook for producing custom output of a single field (row).
//----------------------------------------------------------------------

std::string uvm_printer::format_row( const uvm_printer_row_info& row )
{
  return "";
}

//----------------------------------------------------------------------
// member function: format_header
//
//! Hook to override base header with a custom header.
//----------------------------------------------------------------------

std::string uvm_printer::format_header()
{
  return "";
}

//----------------------------------------------------------------------
// member function: format_footer
//
//! Hook to override base footer with a custom footer.
//----------------------------------------------------------------------

std::string uvm_printer::format_footer()
{
  return "";
}

//----------------------------------------------------------------------
// member function: adjust_name
//
//! Prints a field's name, or \p id, which is the full instance name.
//! The intent of the separator is to mark where the leaf name starts if the
//! printer if configured to print only the leaf name of the identifier.
//----------------------------------------------------------------------

std::string uvm_printer::adjust_name( const std::string& id,
                                 const char* scope_separator ) const
{
  if ( (knobs.show_root && (m_scope.depth() == 0 )) || knobs.full_name || id == "...")
    return id;
  return uvm_leaf_scope(id, scope_separator);
}

//----------------------------------------------------------------------
// member function: print_array_header
//
//! Prints the header of an array. This function is called before each
//! individual element is printed. print_array_footer is called to mark the
//! completion of array printing.
//----------------------------------------------------------------------

void uvm_printer::print_array_header( const std::string& name,
                                      int    size,
                                      const std::string& arraytype,
                                      const char* scope_separator ) const
{
  uvm_printer_row_info row_info;

  if(!name.empty())
    m_scope.set_arg(name);

  row_info.level = m_scope.depth();
  row_info.name = adjust_name(m_scope.get(),scope_separator);
  row_info.type_name = arraytype;

  std::ostringstream str;
  str << size;

  row_info.size = str.str();
  row_info.val = "-";

  m_rows.push_back(row_info);
  m_push_element(row_info.name, row_info.type_name, row_info.size, row_info.val,
                 uvm_printer_element::ELEMENT_ARRAY);

  m_scope.down(name);
  m_array_stack.push_back(true);
}

//----------------------------------------------------------------------
// member function: print_array_range
//
//! Prints a range using ellipses for values. This method is used when honoring
//! the array knobs for partial printing of large arrays,
//! uvm_printer_knobs::begin_elements and uvm_printer_knobs::end_elements.
//!
//! This function should be called after #begin_elements have been printed
//! and before #end_elements have been printed.
//----------------------------------------------------------------------

void uvm_printer::print_array_range( int min, int max ) const
{
  std::string tmpstr;
  if(min == -1 && max == -1)
     return;
  if(min == -1)
     min = max;
  if(max == -1)
     max = min;
  if(max < min)
     return;
  print_generic(std::string("..."), std::string("..."), -2, std::string("..."));
}

//----------------------------------------------------------------------
// member function: print_array_footer
//
//! Prints the header of a footer. This function marks the end of an array
//! print. Generally, there is no output associated with the array footer, but
//! this method lets the printer know that the array printing is complete.
//----------------------------------------------------------------------

void uvm_printer::print_array_footer( int size ) const
{
  (void)size;
  if(m_array_stack.size())
  {
    m_scope.up();
    m_array_stack.pop_front();
    pop_element();
  }
}


////////////////////////////////////////////////////////////////////////
//////// Implementation-defined member functions start here ////////////
////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------

uvm_printer::uvm_printer( const std::string& name )
: uvm_policy(name)
{}

uvm_printer::uvm_printer( const std::string& name, no_inst_count_t )
: uvm_policy(name, no_inst_count_t())
{}

uvm_printer::uvm_printer( const uvm_printer& rhs )
: uvm_policy(rhs),
  knobs(rhs.knobs),
  m_scope(rhs.m_scope),
  m_string(rhs.m_string),
  m_rows(rhs.m_rows),
  m_array_stack(rhs.m_array_stack)
{
  m_copy_elements(rhs);
}

uvm_printer& uvm_printer::operator=( const uvm_printer& rhs )
{
  if (this == &rhs)
    return *this;

  m_cleanup_elements();
  uvm_policy::operator=(rhs);
  knobs = rhs.knobs;
  m_scope = rhs.m_scope;
  m_string = rhs.m_string;
  m_rows = rhs.m_rows;
  m_array_stack = rhs.m_array_stack;
  m_copy_elements(rhs);
  return *this;
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------

uvm_printer::~uvm_printer()
{
  m_cleanup_elements();
}

void uvm_printer::flush()
{
  m_cleanup_elements();
  m_rows.clear();
  m_array_stack.clear();
  m_string.clear();
  m_scope.m_stack.clear();
  m_scope.m_arg.clear();
  uvm_policy::flush();
}

void uvm_printer::m_cleanup_elements() const
{
  for (std::size_t i = 0; i < m_top_elements.size(); ++i)
    delete m_top_elements[i];
  m_top_elements.clear();
  m_element_stack.clear();
}

void uvm_printer::m_copy_elements( const uvm_printer& rhs )
{
  std::map<const uvm_printer_element*, uvm_printer_element*> ptr_map;
  m_top_elements.reserve(rhs.m_top_elements.size());
  for (std::size_t i = 0; i < rhs.m_top_elements.size(); ++i)
  {
    if (rhs.m_top_elements[i] != nullptr)
    {
      uvm_printer_element* clone = new uvm_printer_element(*rhs.m_top_elements[i]);
      record_clone_map(rhs.m_top_elements[i], clone, ptr_map);
      m_top_elements.push_back(clone);
    }
  }
  m_element_stack.reserve(rhs.m_element_stack.size());
  for (std::size_t i = 0; i < rhs.m_element_stack.size(); ++i)
  {
    std::map<const uvm_printer_element*, uvm_printer_element*>::const_iterator it =
      ptr_map.find(rhs.m_element_stack[i]);
    if (it != ptr_map.end())
      m_element_stack.push_back(it->second);
  }
}

void uvm_printer::m_push_element( const std::string& name,
                                  const std::string& type_name,
                                  const std::string& size,
                                  const std::string& value,
                                  uvm_printer_element::element_kind_e kind ) const
{
  uvm_printer_element* elem = new uvm_printer_element(name.empty() ? "uvm_printer_element" : name);
  elem->set(name, type_name, size, value);
  elem->set_element_kind(kind);

  if (m_element_stack.empty())
    m_top_elements.push_back(elem);
  else
    m_element_stack.back()->add_child(elem);

  m_element_stack.push_back(elem);
}

void uvm_printer::push_element( const std::string& name,
                                const std::string& type_name,
                                const std::string& size,
                                const std::string& value ) const
{
  m_push_element(name, type_name, size, value, uvm_printer_element::ELEMENT_FIELD);

  uvm_printer_row_info row_info;
  row_info.level = static_cast<int>(m_element_stack.size()) - 1;
  row_info.name = name;
  row_info.type_name = type_name;
  row_info.size = size;
  row_info.val = value;
  m_rows.push_back(row_info);
}

void uvm_printer::pop_element() const
{
  if (!m_element_stack.empty())
    m_element_stack.pop_back();
}

uvm_printer_element* uvm_printer::get_bottom_element() const
{
  return m_element_stack.empty() ? nullptr : m_element_stack.back();
}

uvm_printer_element* uvm_printer::get_top_element() const
{
  if (!m_element_stack.empty())
    return m_element_stack.front();
  return m_top_elements.empty() ? nullptr : m_top_elements.front();
}

const std::vector<uvm_printer_element*>& uvm_printer::get_top_elements() const
{
  return m_top_elements;
}

uvm_printer* uvm_printer::get_default()
{
  if (uvm_default_printer == nullptr)
    uvm_default_printer = uvm_default_table_printer;
  return uvm_default_printer;
}

void uvm_printer::set_default( uvm_printer* printer )
{
  uvm_default_printer = (printer != nullptr) ? printer : uvm_default_table_printer;
}

//----------------------------------------------------------------------
// Utility methods - Implementation defined
//----------------------------------------------------------------------


//----------------------------------------------------------------------
// member function: istop
//----------------------------------------------------------------------

bool uvm_printer::istop() const
{
  return (m_scope.depth() == 0);
}


} /* namespace uvm */
