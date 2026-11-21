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

#ifndef UVM_POLICY_BASE_H_
#define UVM_POLICY_BASE_H_

#include <map>
#include <string>
#include <vector>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/factory/uvm_abstract_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"

namespace uvm {

class uvm_object_wrapper;

//----------------------------------------------------------------------
// CLASS: uvm_policy
//
//! Base class for policy objects (copier, comparer, packer, etc.)
//! providing active-object stack tracking and non-owning policy
//! extension management per IEEE 1800.2-2020 section 16.1.
//----------------------------------------------------------------------

class uvm_policy : public uvm_object
{
 public:
  UVM_OBJECT_ABSTRACT_UTILS(uvm_policy);

  virtual ~uvm_policy();

  //--------------------------------------------------------------------------
  // Group: Active Object Stack
  //--------------------------------------------------------------------------

  virtual void push_active_object( uvm_object* obj );
  virtual uvm_object* pop_active_object();
  virtual uvm_object* get_active_object() const;

  unsigned int get_active_object_depth() const;
  bool is_active_object( const uvm_object* obj ) const;

  virtual void flush();

  //--------------------------------------------------------------------------
  // Group: Policy Extensions
  //--------------------------------------------------------------------------

  virtual void set_extension( uvm_object* extension );
  virtual bool extension_exists( const uvm_object_wrapper* extension_type ) const;
  virtual uvm_object* get_extension( const uvm_object_wrapper* extension_type ) const;
  virtual void clear_extension( const uvm_object_wrapper* extension_type );
  virtual void clear_extensions();

 protected:
  explicit uvm_policy( const std::string& name = "" );

 private:
  std::vector<uvm_object*> m_policy_stack;
  std::map<const uvm_object_wrapper*, uvm_object*> m_policy_extensions;
};

} // namespace uvm

#endif /* UVM_POLICY_BASE_H_ */
