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

#include "uvmsc/factory/uvm_factory.h"
#include "uvmsc/policy/uvm_copier.h"
#include "uvmsc/policy/uvm_field_op.h"

namespace uvm {

namespace {

class active_object_guard
{
 public:
  active_object_guard( uvm_policy* policy, uvm_object* obj )
  : m_policy(policy)
  {
    m_policy->push_active_object(obj);
  }

  ~active_object_guard()
  {
    m_policy->pop_active_object();
  }

 private:
  active_object_guard( const active_object_guard& );
  active_object_guard& operator=( const active_object_guard& );

  uvm_policy* m_policy;
};

class field_op_guard
{
 public:
  explicit field_op_guard( uvm_field_op* op )
  : m_op(op)
  {}

  ~field_op_guard()
  {
    if (m_op != nullptr)
      m_op->m_recycle();
  }

 private:
  field_op_guard( const field_op_guard& );
  field_op_guard& operator=( const field_op_guard& );

  uvm_field_op* m_op;
};

class copy_rollback_guard
{
 public:
  copy_rollback_guard( uvm_copier* copier,
                       void (uvm_copier::*rollback_fn)(uvm_object*, const uvm_object*, uvm_object*),
                       uvm_object* lhs,
                       const uvm_object* rhs,
                       uvm_object* prev_mapped )
  : m_copier(copier),
    m_rollback_fn(rollback_fn),
    m_lhs(lhs),
    m_rhs(rhs),
    m_prev_mapped(prev_mapped),
    m_committed(false)
  {}

  void commit()
  {
    m_committed = true;
  }

  ~copy_rollback_guard()
  {
    if (!m_committed && m_copier != nullptr)
      (m_copier->*m_rollback_fn)(m_lhs, m_rhs, m_prev_mapped);
  }

 private:
  copy_rollback_guard( const copy_rollback_guard& );
  copy_rollback_guard& operator=( const copy_rollback_guard& );

  uvm_copier* m_copier;
  void (uvm_copier::*m_rollback_fn)(uvm_object*, const uvm_object*, uvm_object*);
  uvm_object* m_lhs;
  const uvm_object* m_rhs;
  uvm_object* m_prev_mapped;
  bool m_committed;
};

class allocated_object_guard
{
 public:
  explicit allocated_object_guard( uvm_object* obj )
  : m_obj(obj)
  {}

  void release()
  {
    m_obj = nullptr;
  }

  ~allocated_object_guard()
  {
    delete m_obj;
  }

 private:
  allocated_object_guard( const allocated_object_guard& );
  allocated_object_guard& operator=( const allocated_object_guard& );

  uvm_object* m_obj;
};

} // namespace

uvm_copier* uvm_default_copier = nullptr;

uvm_copier::uvm_copier( const std::string& name )
: uvm_policy(name),
  m_policy(UVM_DEFAULT_POLICY)
{}

uvm_copier::~uvm_copier()
{
  m_copy_map.clear();
  m_recur_states.clear();
  m_iter_index.clear();
}

void uvm_copier::flush()
{
  m_copy_map.clear();
  m_recur_states.clear();
  m_iter_index.clear();
  uvm_policy::flush();
}

void uvm_copier::copy_object( uvm_object*& lhs, const uvm_object* rhs )
{
  if (rhs == nullptr)
  {
    lhs = nullptr;
    return;
  }

  if (lhs == rhs)
    return;

  if (m_policy == UVM_REFERENCE)
  {
    lhs = const_cast<uvm_object*>(rhs);
    return;
  }

  if (m_policy == UVM_SHALLOW && get_active_object_depth() > 0)
  {
    lhs = const_cast<uvm_object*>(rhs);
    return;
  }

  uvm_object* target = lhs;
  bool newly_allocated = false;

  if (target == nullptr)
  {
    uvm_object* prev = m_copy_map.get(const_cast<uvm_object*>(rhs));
    if (prev != nullptr)
    {
      lhs = prev;
      return;
    }

    const uvm_object_wrapper* wrapper = rhs->get_object_type();
    if (wrapper != nullptr)
    {
      uvm_object_wrapper* resolved = const_cast<uvm_object_wrapper*>(wrapper);
      uvm_factory* factory = uvm_factory::get();
      if (factory != nullptr)
      {
        uvm_object_wrapper* ovr =
          factory->find_override_by_type(resolved, rhs->get_name());
        if (ovr != nullptr)
          resolved = ovr;
      }
      target = resolved->create_object(rhs->get_name());
    }
    if (target == nullptr)
      target = const_cast<uvm_object*>(rhs)->create(rhs->get_name());
    if (target == nullptr)
      return;
    newly_allocated = true;
  }
  else
  {
    std::map<const uvm_object*, std::vector<m_copy_entry> >::const_iterator it =
      m_recur_states.find(rhs);
    if (it != m_recur_states.end())
    {
      for (std::size_t i = 0; i < it->second.size(); ++i)
      {
        if (it->second[i].lhs == target)
          return;
        if (it->second[i].state == STARTED)
        {
          lhs = it->second[i].lhs;
          return;
        }
      }
    }
  }

  if (newly_allocated)
  {
    allocated_object_guard alloc_guard(target);
    m_do_copy_object(target, rhs);
    alloc_guard.release();
    lhs = target;
  }
  else
  {
    m_do_copy_object(target, rhs);
  }
}

void uvm_copier::copy_object( uvm_object& lhs, const uvm_object& rhs )
{
  if (&lhs == &rhs)
    return;

  if (get_active_object_depth() > 0 &&
      (m_policy == UVM_REFERENCE || m_policy == UVM_SHALLOW))
    return;

  std::map<const uvm_object*, std::vector<m_copy_entry> >::const_iterator it =
    m_recur_states.find(&rhs);
  if (it != m_recur_states.end())
  {
    for (std::size_t i = 0; i < it->second.size(); ++i)
    {
      if (it->second[i].lhs == &lhs)
        return;
      if (get_active_object_depth() > 0 && it->second[i].state == STARTED)
        return;
    }
  }

  m_do_copy_object(&lhs, &rhs);
}

void uvm_copier::m_rollback_copy_object( uvm_object* lhs,
                                          const uvm_object* rhs,
                                          uvm_object* prev_mapped )
{
  uvm_object* key = const_cast<uvm_object*>(rhs);
  if (prev_mapped != nullptr)
    m_copy_map.set(key, prev_mapped);
  else
    m_copy_map.erase(key);

  std::map<const uvm_object*, std::vector<m_copy_entry> >::iterator it =
    m_recur_states.find(rhs);
  if (it != m_recur_states.end())
  {
    std::vector<m_copy_entry>& entries = it->second;
    for (std::size_t i = entries.size(); i > 0; --i)
    {
      if (entries[i - 1].lhs == lhs && entries[i - 1].state == STARTED)
      {
        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(i - 1));
        break;
      }
    }
    if (entries.empty())
    {
      m_recur_states.erase(it);
      m_iter_index.erase(rhs);
    }
  }
}

void uvm_copier::m_do_copy_object( uvm_object* lhs, const uvm_object* rhs )
{
  uvm_object* prev_mapped = m_copy_map.get(const_cast<uvm_object*>(rhs));
  copy_rollback_guard rollback_guard(
    this, &uvm_copier::m_rollback_copy_object, lhs, rhs, prev_mapped);

  m_copy_entry entry;
  entry.lhs = lhs;
  entry.state = STARTED;

  m_recur_states[rhs].push_back(entry);
  m_copy_map.set(const_cast<uvm_object*>(rhs), lhs);

  {
    active_object_guard active_guard(this, lhs);
    uvm_field_op* op = uvm_field_op::m_get_available_op();
    field_op_guard op_guard(op);

    op->set(UVM_COPY, this, rhs);
    lhs->do_execute_op(op);
    if (op->user_hook_enabled())
      lhs->do_copy(*rhs);
  }

  std::vector<m_copy_entry>& entries = m_recur_states[rhs];
  for (std::size_t i = entries.size(); i > 0; --i)
  {
    if (entries[i - 1].lhs == lhs)
    {
      entries[i - 1].state = FINISHED;
      break;
    }
  }
  rollback_guard.commit();
}

int uvm_copier::object_copied( const uvm_object* lhs, const uvm_object* rhs ) const
{
  if (lhs == nullptr || rhs == nullptr)
    return CREATED;

  std::map<const uvm_object*, std::vector<m_copy_entry> >::const_iterator it =
    m_recur_states.find(rhs);
  if (it == m_recur_states.end())
    return CREATED;

  for (std::size_t i = 0; i < it->second.size(); ++i)
  {
    if (it->second[i].lhs == lhs)
      return static_cast<int>(it->second[i].state);
  }

  return CREATED;
}

bool uvm_copier::get_first_copy( const uvm_object* rhs, uvm_object*& lhs ) const
{
  if (rhs == nullptr)
  {
    lhs = nullptr;
    return false;
  }

  std::map<const uvm_object*, std::vector<m_copy_entry> >::const_iterator it =
    m_recur_states.find(rhs);
  if (it == m_recur_states.end() || it->second.empty())
  {
    lhs = nullptr;
    return false;
  }

  m_iter_index[rhs] = 0;
  lhs = it->second[0].lhs;
  return true;
}

bool uvm_copier::get_next_copy( const uvm_object* rhs, uvm_object*& lhs ) const
{
  if (rhs == nullptr)
  {
    lhs = nullptr;
    return false;
  }

  std::map<const uvm_object*, std::vector<m_copy_entry> >::const_iterator it =
    m_recur_states.find(rhs);
  if (it == m_recur_states.end() || it->second.empty())
  {
    lhs = nullptr;
    return false;
  }

  std::map<const uvm_object*, std::size_t>::iterator idx_it = m_iter_index.find(rhs);
  if (idx_it == m_iter_index.end())
  {
    lhs = nullptr;
    return false;
  }

  std::size_t next_idx = idx_it->second + 1;
  if (next_idx >= it->second.size())
  {
    lhs = nullptr;
    return false;
  }

  idx_it->second = next_idx;
  lhs = it->second[next_idx].lhs;
  return true;
}

bool uvm_copier::get_last_copy( const uvm_object* rhs, uvm_object*& lhs ) const
{
  if (rhs == nullptr)
  {
    lhs = nullptr;
    return false;
  }

  std::map<const uvm_object*, std::vector<m_copy_entry> >::const_iterator it =
    m_recur_states.find(rhs);
  if (it == m_recur_states.end() || it->second.empty())
  {
    lhs = nullptr;
    return false;
  }

  lhs = it->second.back().lhs;
  return true;
}

int uvm_copier::get_num_copies( const uvm_object* rhs ) const
{
  if (rhs == nullptr)
    return 0;

  std::map<const uvm_object*, std::vector<m_copy_entry> >::const_iterator it =
    m_recur_states.find(rhs);
  if (it == m_recur_states.end())
    return 0;

  return static_cast<int>(it->second.size());
}

void uvm_copier::set_recursion_policy( uvm_recursion_policy_enum policy )
{
  m_policy = policy;
}

uvm_recursion_policy_enum uvm_copier::get_recursion_policy() const
{
  return m_policy;
}

uvm_copier& uvm_copier::m_builtin_default()
{
  static uvm_copier builtin_default("uvm_default_copier");
  return builtin_default;
}

uvm_copier* uvm_copier::get_default()
{
  if (uvm_default_copier == nullptr)
    uvm_default_copier = &m_builtin_default();
  return uvm_default_copier;
}

void uvm_copier::set_default( uvm_copier* copier )
{
  uvm_default_copier = (copier != nullptr) ? copier : &m_builtin_default();
}

} // namespace uvm
