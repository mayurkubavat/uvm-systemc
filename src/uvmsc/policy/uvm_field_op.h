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

#ifndef UVM_FIELD_OP_H_
#define UVM_FIELD_OP_H_

#include <string>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_object_globals.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"
#include "uvmsc/policy/uvm_policy_base.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_field_op
//
//! Encapsulates field-operation context (op type, policy, rhs, and
//! user-hook enable state) passed to uvm_object::do_execute_op() per
//! IEEE 1800.2-2020 section 16.2.
//----------------------------------------------------------------------

class uvm_field_op : public uvm_object
{
 public:
  UVM_OBJECT_UTILS(uvm_field_op);

  explicit uvm_field_op( const std::string& name = "uvm_field_op" );
  virtual ~uvm_field_op();

  virtual void set( uvm_field_op_type op_type,
                    uvm_policy* policy = nullptr,
                    const uvm_object* rhs = nullptr );

  virtual uvm_field_op_type get_op_type() const;
  virtual const uvm_object* get_rhs() const;
  virtual uvm_policy* get_policy() const;

  virtual bool user_hook_enabled() const;
  virtual void disable_user_hook();

  virtual void flush();

  static uvm_field_op* m_get_available_op();
  void m_recycle();

 private:
  uvm_field_op_type m_op_type;
  uvm_policy* m_policy;
  const uvm_object* m_rhs;
  bool m_user_hook;
  bool m_is_set;
  bool m_is_pooled;
};

} // namespace uvm

#endif /* UVM_FIELD_OP_H_ */
