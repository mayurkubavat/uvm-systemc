//----------------------------------------------------------------------
//   Copyright 2012-2014 NXP B.V.
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

#ifndef UVM_TREE_PRINTER_H_
#define UVM_TREE_PRINTER_H_

#include <string>
#include <systemc>

#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"
#include "uvmsc/print/uvm_printer.h"


//////////////

namespace uvm {


//------------------------------------------------------------------------------
// Class: uvm_tree_printer
//
//! By overriding various methods of the #uvm_printer base class,
//! the tree printer prints output in a tree format.
//------------------------------------------------------------------------------

class uvm_tree_printer: public uvm_printer
{
 public:
  UVM_OBJECT_UTILS(uvm_tree_printer);

  explicit uvm_tree_printer( const std::string& name = "uvm_tree_printer" );
  virtual ~uvm_tree_printer();
  virtual std::string emit();
  static uvm_tree_printer* m_create_default();
  std::string newline;

 protected:
  uvm_tree_printer( const std::string& name, no_inst_count_t );
};

//////////////


} // namespace uvm

#endif /* UVM_TREE_PRINTER_H_ */
