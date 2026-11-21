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

#ifndef UVM_PRINTER_ELEMENT_H_
#define UVM_PRINTER_ELEMENT_H_

#include <string>
#include <vector>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_traversal.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_printer_element
//
//! Tree node representing a printed field, object, array, or array range
//! per IEEE 1800.2-2020 section 16.3.4.
//----------------------------------------------------------------------

class uvm_printer_element : public uvm_object
{
 public:
  enum element_kind_e
  {
    ELEMENT_FIELD = 0,
    ELEMENT_OBJECT,
    ELEMENT_ARRAY,
    ELEMENT_ARRAY_RANGE
  };

  UVM_OBJECT_UTILS(uvm_printer_element);

  explicit uvm_printer_element( const std::string& name = "uvm_printer_element" );
  uvm_printer_element( const uvm_printer_element& rhs );
  uvm_printer_element& operator=( const uvm_printer_element& rhs );
  virtual ~uvm_printer_element();

  virtual void set( const std::string& element_name = "",
                    const std::string& element_type_name = "",
                    const std::string& element_size = "",
                    const std::string& element_value = "" );

  virtual void set_element_name( const std::string& element_name );
  virtual std::string get_element_name() const;

  virtual void set_element_type_name( const std::string& element_type_name );
  virtual std::string get_element_type_name() const;

  virtual void set_element_size( const std::string& element_size );
  virtual std::string get_element_size() const;

  virtual void set_element_value( const std::string& element_value );
  virtual std::string get_element_value() const;

  virtual void add_child( uvm_printer_element* child );
  virtual void get_children( std::vector<uvm_printer_element*>& children,
                             bool recurse = false ) const;
  const std::vector<uvm_printer_element*>& get_children() const;
  virtual void clear_children();
  virtual bool has_children() const;
  virtual uvm_printer_element* get_parent() const;

  void set_element_kind( element_kind_e kind );
  element_kind_e get_element_kind() const;

 private:
  void m_collect_children_recursive( std::vector<uvm_printer_element*>& children ) const;

  std::string m_elem_name;
  std::string m_elem_type_name;
  std::string m_elem_size;
  std::string m_elem_value;
  element_kind_e m_kind;
  uvm_printer_element* m_parent;
  std::vector<uvm_printer_element*> m_children;
};

//----------------------------------------------------------------------
// CLASS: uvm_printer_element_proxy
//
//! Structure proxy for traversing uvm_printer_element trees via
//! uvm_visitor_adapter per IEEE 1800.2-2020 section 16.3.5.
//----------------------------------------------------------------------

class uvm_printer_element_proxy : public uvm_structure_proxy<uvm_printer_element>
{
 public:
  UVM_OBJECT_UTILS(uvm_printer_element_proxy);

  explicit uvm_printer_element_proxy( const std::string& name = "uvm_printer_element_proxy" );
  virtual ~uvm_printer_element_proxy();

  virtual void get_immediate_children( uvm_printer_element* s,
                                       std::vector<uvm_printer_element*>& children );
};

} // namespace uvm

#endif /* UVM_PRINTER_ELEMENT_H_ */
