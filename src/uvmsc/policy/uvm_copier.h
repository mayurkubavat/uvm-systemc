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

#ifndef UVM_COPIER_H_
#define UVM_COPIER_H_

#include <cstddef>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "uvmsc/base/uvm_object_globals.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"
#include "uvmsc/misc/uvm_copy_map.h"
#include "uvmsc/policy/uvm_policy_base.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_copier
//
//! Policy class controlling object copy semantics (deep, shallow,
//! reference), cycle/DAG tracking, and copy history queries per
//! IEEE 1800.2-2020 section 16.4.
//----------------------------------------------------------------------

class uvm_copier : public uvm_policy
{
 public:
  UVM_OBJECT_UTILS(uvm_copier);

  enum recursion_state_e
  {
    CREATED  = 0,
    STARTED  = 1,
    FINISHED = 2
  };

  explicit uvm_copier( const std::string& name = "uvm_copier" );
  virtual ~uvm_copier();

  // Policy Reset
  virtual void flush();

  // Copy API (IEEE 1800.2-2020 section 16.4.3)
  virtual void copy_object( uvm_object*& lhs, const uvm_object* rhs );

  template <typename T,
            typename std::enable_if<
              std::is_base_of<uvm_object, T>::value &&
              !std::is_same<uvm_object, T>::value, int>::type = 0>
  void copy_object( T*& lhs, const T* rhs )
  {
    uvm_object* tmp = lhs;
    copy_object(tmp, static_cast<const uvm_object*>(rhs));
    lhs = (tmp == nullptr) ? nullptr : dynamic_cast<T*>(tmp);
  }

  void copy_object( uvm_object& lhs, const uvm_object& rhs );

  virtual int object_copied( const uvm_object* lhs, const uvm_object* rhs ) const;

  virtual bool get_first_copy( const uvm_object* rhs, uvm_object*& lhs ) const;
  virtual bool get_next_copy( const uvm_object* rhs, uvm_object*& lhs ) const;
  virtual bool get_last_copy( const uvm_object* rhs, uvm_object*& lhs ) const;
  virtual int get_num_copies( const uvm_object* rhs ) const;

  // Recursion Policy Configuration (IEEE 1800.2-2020 section 16.4.2)
  virtual void set_recursion_policy( uvm_recursion_policy_enum policy );
  virtual uvm_recursion_policy_enum get_recursion_policy() const;

  // Default Copier
  static uvm_copier* get_default();
  static void set_default( uvm_copier* copier );

 private:
  struct m_copy_entry
  {
    uvm_object* lhs;
    recursion_state_e state;
  };

  void m_do_copy_object( uvm_object* lhs, const uvm_object* rhs );
  void m_rollback_copy_object( uvm_object* lhs,
                               const uvm_object* rhs,
                               uvm_object* prev_mapped );
  static uvm_copier& m_builtin_default();

  uvm_recursion_policy_enum m_policy;
  uvm_copy_map m_copy_map;
  std::map<const uvm_object*, std::vector<m_copy_entry> > m_recur_states;
  mutable std::map<const uvm_object*, std::size_t> m_iter_index;
};

extern uvm_copier* uvm_default_copier;

} // namespace uvm

#endif /* UVM_COPIER_H_ */
