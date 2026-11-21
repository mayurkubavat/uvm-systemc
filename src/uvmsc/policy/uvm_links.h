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

#ifndef UVM_LINKS_H_
#define UVM_LINKS_H_

#include <string>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_link_base
//
//! The base of a link between two objects of a transaction database,
//! such as two recorders. A database establishes it with
//! uvm_tr_database::establish_link(); it does not keep the link.
//----------------------------------------------------------------------

class uvm_link_base : public uvm_object
{
 public:
  explicit uvm_link_base( const std::string& name = "unnamed-uvm_link_base" );

  void set_lhs( uvm_object* lhs );
  uvm_object* get_lhs() const;

  void set_rhs( uvm_object* rhs );
  uvm_object* get_rhs() const;

  void set( uvm_object* lhs, uvm_object* rhs );

 protected:
  virtual void do_set_lhs( uvm_object* lhs ) = 0;
  virtual uvm_object* do_get_lhs() const = 0;
  virtual void do_set_rhs( uvm_object* rhs ) = 0;
  virtual uvm_object* do_get_rhs() const = 0;
};

//----------------------------------------------------------------------
// CLASS: uvm_parent_child_link
//
//! A link from a parent (the left hand side) to a child (the right hand
//! side)
//----------------------------------------------------------------------

class uvm_parent_child_link : public uvm_link_base
{
 public:
  UVM_OBJECT_UTILS(uvm_parent_child_link);

  explicit uvm_parent_child_link( const std::string& name = "unnamed-uvm_parent_child_link" );

  //! Creates a link; the caller owns it
  static uvm_parent_child_link* get_link( uvm_object* lhs, uvm_object* rhs,
                                          const std::string& name = "pc_link" );

 protected:
  virtual void do_set_lhs( uvm_object* lhs );
  virtual uvm_object* do_get_lhs() const;
  virtual void do_set_rhs( uvm_object* rhs );
  virtual uvm_object* do_get_rhs() const;

 private:
  uvm_object* m_lhs;
  uvm_object* m_rhs;
};

//----------------------------------------------------------------------
// CLASS: uvm_cause_effect_link
//
//! A link from a cause (the left hand side) to an effect (the right
//! hand side)
//----------------------------------------------------------------------

class uvm_cause_effect_link : public uvm_link_base
{
 public:
  UVM_OBJECT_UTILS(uvm_cause_effect_link);

  explicit uvm_cause_effect_link( const std::string& name = "unnamed-uvm_cause_effect_link" );

  //! Creates a link; the caller owns it
  static uvm_cause_effect_link* get_link( uvm_object* lhs, uvm_object* rhs,
                                          const std::string& name = "ce_link" );

 protected:
  virtual void do_set_lhs( uvm_object* lhs );
  virtual uvm_object* do_get_lhs() const;
  virtual void do_set_rhs( uvm_object* rhs );
  virtual uvm_object* do_get_rhs() const;

 private:
  uvm_object* m_lhs;
  uvm_object* m_rhs;
};

//----------------------------------------------------------------------
// CLASS: uvm_related_link
//
//! A link between two related objects, with no direction
//----------------------------------------------------------------------

class uvm_related_link : public uvm_link_base
{
 public:
  UVM_OBJECT_UTILS(uvm_related_link);

  explicit uvm_related_link( const std::string& name = "unnamed-uvm_related_link" );

  //! Creates a link; the caller owns it
  static uvm_related_link* get_link( uvm_object* lhs, uvm_object* rhs,
                                     const std::string& name = "ce_link" );

 protected:
  virtual void do_set_lhs( uvm_object* lhs );
  virtual uvm_object* do_get_lhs() const;
  virtual void do_set_rhs( uvm_object* rhs );
  virtual uvm_object* do_get_rhs() const;

 private:
  uvm_object* m_lhs;
  uvm_object* m_rhs;
};

} // namespace uvm

#endif // UVM_LINKS_H_
