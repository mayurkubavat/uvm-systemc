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

#include <memory>
#include <vector>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/policy/uvm_field_op.h"

namespace uvm {

namespace {

std::vector<std::unique_ptr<uvm_field_op> >& field_op_pool()
{
  static std::vector<std::unique_ptr<uvm_field_op> > pool;
  return pool;
}

} // namespace

uvm_field_op::uvm_field_op( const std::string& name )
: uvm_object(name),
  m_op_type(UVM_COPY),
  m_policy(nullptr),
  m_rhs(nullptr),
  m_user_hook(true),
  m_is_set(false),
  m_is_pooled(false)
{}

uvm_field_op::~uvm_field_op()
{}

void uvm_field_op::set( uvm_field_op_type op_type,
                        uvm_policy* policy,
                        const uvm_object* rhs )
{
  if (m_is_set)
  {
    uvm_report_error("UVM/FIELD_OP/SET",
                     "uvm_field_op::set() called when op is already set; call flush() first",
                     UVM_NONE);
    return;
  }

  m_op_type = op_type;
  m_policy = policy;
  m_rhs = rhs;
  m_user_hook = true;
  m_is_set = true;
}

uvm_field_op_type uvm_field_op::get_op_type() const
{
  if (!m_is_set)
  {
    uvm_report_error("UVM/FIELD_OP/NO_SET",
                     "uvm_field_op::get_op_type() called before set()",
                     UVM_NONE);
    return m_op_type;
  }
  return m_op_type;
}

const uvm_object* uvm_field_op::get_rhs() const
{
  if (!m_is_set)
  {
    uvm_report_error("UVM/FIELD_OP/NO_SET",
                     "uvm_field_op::get_rhs() called before set()",
                     UVM_NONE);
    return nullptr;
  }
  return m_rhs;
}

uvm_policy* uvm_field_op::get_policy() const
{
  if (!m_is_set)
  {
    uvm_report_error("UVM/FIELD_OP/NO_SET",
                     "uvm_field_op::get_policy() called before set()",
                     UVM_NONE);
    return nullptr;
  }
  return m_policy;
}

bool uvm_field_op::user_hook_enabled() const
{
  if (!m_is_set)
  {
    uvm_report_error("UVM/FIELD_OP/NO_SET",
                     "uvm_field_op::user_hook_enabled() called before set()",
                     UVM_NONE);
    return false;
  }
  return m_user_hook;
}

void uvm_field_op::disable_user_hook()
{
  if (!m_is_set)
  {
    uvm_report_error("UVM/FIELD_OP/NO_SET",
                     "uvm_field_op::disable_user_hook() called before set()",
                     UVM_NONE);
    return;
  }
  m_user_hook = false;
}

void uvm_field_op::flush()
{
  m_op_type = UVM_COPY;
  m_policy = nullptr;
  m_rhs = nullptr;
  m_user_hook = true;
  m_is_set = false;
}

uvm_field_op* uvm_field_op::m_get_available_op()
{
  std::vector<std::unique_ptr<uvm_field_op> >& pool = field_op_pool();
  if (!pool.empty())
  {
    uvm_field_op* op = pool.back().release();
    pool.pop_back();
    op->flush();
    op->m_is_pooled = true;
    return op;
  }

  uvm_field_op* op = new uvm_field_op("uvm_field_op");
  op->m_is_pooled = true;
  return op;
}

void uvm_field_op::m_recycle()
{
  if (!m_is_pooled)
    return;

  m_is_pooled = false;
  flush();
  field_op_pool().push_back(std::unique_ptr<uvm_field_op>(this));
}

} // namespace uvm
