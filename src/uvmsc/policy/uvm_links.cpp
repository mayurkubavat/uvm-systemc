//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2007-2014 Mentor Graphics Corporation
//   Copyright 2013-2024 NVIDIA Corporation
//   Copyright 2013 NXP B.V.
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

#include "uvmsc/policy/uvm_links.h"

namespace uvm {

uvm_link_base::uvm_link_base( const std::string& name ) : uvm_object(name) {}

void uvm_link_base::set_lhs( uvm_object* lhs ) { do_set_lhs(lhs); }
uvm_object* uvm_link_base::get_lhs() const { return do_get_lhs(); }
void uvm_link_base::set_rhs( uvm_object* rhs ) { do_set_rhs(rhs); }
uvm_object* uvm_link_base::get_rhs() const { return do_get_rhs(); }

void uvm_link_base::set( uvm_object* lhs, uvm_object* rhs )
{
  do_set_lhs(lhs);
  do_set_rhs(rhs);
}

#define M_UVM_LINK_IMPL(TYPE) \
  TYPE::TYPE( const std::string& name ) \
  : uvm_link_base(name), m_lhs(nullptr), m_rhs(nullptr) {} \
  TYPE* TYPE::get_link( uvm_object* lhs, uvm_object* rhs, const std::string& name ) \
  { \
    TYPE* link = new TYPE(name); \
    link->set(lhs, rhs); \
    return link; \
  } \
  void TYPE::do_set_lhs( uvm_object* lhs ) { m_lhs = lhs; } \
  uvm_object* TYPE::do_get_lhs() const { return m_lhs; } \
  void TYPE::do_set_rhs( uvm_object* rhs ) { m_rhs = rhs; } \
  uvm_object* TYPE::do_get_rhs() const { return m_rhs; }

M_UVM_LINK_IMPL(uvm_parent_child_link)
M_UVM_LINK_IMPL(uvm_cause_effect_link)
M_UVM_LINK_IMPL(uvm_related_link)

#undef M_UVM_LINK_IMPL

} // namespace uvm
