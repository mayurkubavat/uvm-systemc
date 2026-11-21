//----------------------------------------------------------------------
//   Copyright 2018 Cadence Design Systems, Inc.
//   Copyright 2018-2024 NVIDIA Corporation
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

#include <algorithm>

#include "uvmsc/base/uvm_run_test_callback.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_run_test_callback
//----------------------------------------------------------------------

uvm_run_test_callback::uvm_run_test_callback( const std::string& name )
: uvm_callback(name)
{}

//----------------------------------------------------------------------
// member function: add (static)
//
//! Registers the callback cb. Returns true if the callback was added,
//! false if cb is a null pointer or was already registered.
//----------------------------------------------------------------------

bool uvm_run_test_callback::add( uvm_run_test_callback* cb )
{
  if (cb == nullptr)
    return false;

  std::vector<uvm_run_test_callback*>& cbs = m_registered_cbs();
  if (std::find(cbs.begin(), cbs.end(), cb) != cbs.end())
    return false;

  cbs.push_back(cb);
  return true;
}

//----------------------------------------------------------------------
// member function: do_delete (static)
//
//! Removes the registered callback cb. Returns true if the callback was
//! removed, false if cb is a null pointer or was not registered.
//----------------------------------------------------------------------

bool uvm_run_test_callback::do_delete( uvm_run_test_callback* cb )
{
  if (cb == nullptr)
    return false;

  std::vector<uvm_run_test_callback*>& cbs = m_registered_cbs();
  std::vector<uvm_run_test_callback*>::iterator it =
    std::remove(cbs.begin(), cbs.end(), cb);

  if (it == cbs.end())
    return false;

  cbs.erase(it, cbs.end());
  return true;
}

const std::string uvm_run_test_callback::get_type_name() const
{
  return "uvm::uvm_run_test_callback";
}

uvm_run_test_callback::~uvm_run_test_callback()
{
  do_delete(this);
}

////////////////////////////////////////////////////////////////////////
//////// Implementation-defined member functions start here ////////////
////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------
// member functions: m_do_pre_run_test, m_do_post_run_test,
// m_do_pre_abort (static)
//
//! Implementation defined
//! Execute pre_run_test, post_run_test or pre_abort of all registered
//! callbacks, in the order of registration. A callback may add or
//! delete callbacks: the callbacks registered at the start are called,
//! except those which are no longer registered when it is their turn, so
//! that a callback which is deleted may also be destroyed.
//----------------------------------------------------------------------

void uvm_run_test_callback::m_do_pre_run_test()
{
  const std::vector<uvm_run_test_callback*>& registered = m_registered_cbs();
  std::vector<uvm_run_test_callback*> cbs = registered;
  for (std::size_t i = 0; i < cbs.size(); i++)
    if (std::find(registered.begin(), registered.end(), cbs[i]) != registered.end())
      cbs[i]->pre_run_test();
}

void uvm_run_test_callback::m_do_post_run_test()
{
  const std::vector<uvm_run_test_callback*>& registered = m_registered_cbs();
  std::vector<uvm_run_test_callback*> cbs = registered;
  for (std::size_t i = 0; i < cbs.size(); i++)
    if (std::find(registered.begin(), registered.end(), cbs[i]) != registered.end())
      cbs[i]->post_run_test();
}

void uvm_run_test_callback::m_do_pre_abort()
{
  const std::vector<uvm_run_test_callback*>& registered = m_registered_cbs();
  std::vector<uvm_run_test_callback*> cbs = registered;
  for (std::size_t i = 0; i < cbs.size(); i++)
    if (std::find(registered.begin(), registered.end(), cbs[i]) != registered.end())
      cbs[i]->pre_abort();
}

//----------------------------------------------------------------------
// member function: m_registered_cbs (static)
//
//! Implementation defined
//! Returns the registered callbacks. The list is never destroyed, so
//! that callbacks with static storage duration can remove themselves
//! from the list in their destructor.
//----------------------------------------------------------------------

std::vector<uvm_run_test_callback*>& uvm_run_test_callback::m_registered_cbs()
{
  static std::vector<uvm_run_test_callback*>* registered_cbs =
    new std::vector<uvm_run_test_callback*>();
  return *registered_cbs;
}

} // namespace uvm
