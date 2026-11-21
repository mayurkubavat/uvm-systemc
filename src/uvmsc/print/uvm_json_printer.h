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

#ifndef UVM_JSON_PRINTER_H_
#define UVM_JSON_PRINTER_H_

#include <string>

#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"
#include "uvmsc/print/uvm_printer.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_json_printer
//
//! Serializes uvm_printer_element trees into RFC 8259 JSON format.
//----------------------------------------------------------------------

class uvm_json_printer : public uvm_printer
{
 public:
  UVM_OBJECT_UTILS(uvm_json_printer);

  explicit uvm_json_printer( const std::string& name = "uvm_json_printer" );
  virtual ~uvm_json_printer();

  virtual std::string emit();

  static uvm_json_printer* get_default();
  static void set_default( uvm_json_printer* printer );

 private:
  static uvm_json_printer& m_builtin_default();
};

extern uvm_json_printer* uvm_default_json_printer;

} // namespace uvm

#endif /* UVM_JSON_PRINTER_H_ */
