//-----------------------------------------------------------------------------
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2010 Cadence Design Systems, Inc. 
//   Copyright 2010 Synopsys, Inc.
//   Copyright 2014-2015 NXP B.V.
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
//-----------------------------------------------------------------------------

#ifndef UVM_OBJECT_STRING_POOL_H_
#define UVM_OBJECT_STRING_POOL_H_

#include <map>
#include <string>
#include <vector>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/base/uvm_event.h"
#include "uvmsc/conf/uvm_pool.h"
#include "uvmsc/print/uvm_printer.h"

namespace uvm {

//------------------------------------------------------------------------------
// CLASS: uvm_object_string_pool<T>
//
//! This provides a specialization of the generic #uvm_pool<KEY,T> class for
//! an associative array of #uvm_object-based objects indexed by string. The
//! pool holds pointers to objects of type T, which shall have a constructor
//! taking the name of the object.
//!
//! The pool owns the objects which get() creates, and deletes them when it
//! is deleted itself; an object which is taken out with do_delete() lives
//! until then, as others may still refer to it. Objects which are put in
//! with add() stay with their owner. A copy of a pool made with copy() or
//! clone() refers to the same objects without owning them, so it shall not
//! be used after the pool it was copied from is deleted. A pool cannot be
//! copy-constructed or assigned.
//!
//! Specializations of this class include the #uvm_event_pool (a
//! uvm_object_string_pool storing objects of type #uvm_event) and
//! #uvm_barrier_pool (a #uvm_object_string_pool storing objects of type
//! #uvm_barrier).
//------------------------------------------------------------------------------

template <typename T = uvm_object>
class uvm_object_string_pool : public uvm_pool<std::string, T*>
{
 public:
  typedef uvm_object_string_pool<T> this_type;

  //--------------------------------------------------------------------------
  // UVM Standard LRM API below
  //--------------------------------------------------------------------------

  explicit uvm_object_string_pool( const std::string& name = "" );
  virtual ~uvm_object_string_pool();

  uvm_object_string_pool( const uvm_object_string_pool& ) = delete;
  uvm_object_string_pool& operator=( const uvm_object_string_pool& ) = delete;

  static this_type* get_global_pool();
  static T* get_global( const std::string& key );

  virtual T* get( const std::string& key );
  virtual void do_delete( const std::string& key );

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  virtual uvm_object* create( const std::string& name = "" );

  virtual const std::string get_type_name() const;

  virtual void do_print( const uvm_printer& printer ) const;

 private:
  // the objects which get() created; they are deleted with the pool
  std::vector<T*> m_created;

  static this_type* m_global_pool;

}; // class uvm_object_string_pool


/////////////////////////////////////////
// Class implementation starts here
/////////////////////////////////////////

//------------------------------------------------------------------------------
// Initialization of static data members
//------------------------------------------------------------------------------

template <typename T>
uvm_object_string_pool<T>* uvm_object_string_pool<T>::m_global_pool = nullptr;

//------------------------------------------------------------------------------
// Constructor
//
//! Creates a new pool with the given \p name.
//------------------------------------------------------------------------------

template <typename T>
uvm_object_string_pool<T>::uvm_object_string_pool( const std::string& name )
: uvm_pool<std::string, T*>(name)
{}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------

template <typename T>
uvm_object_string_pool<T>::~uvm_object_string_pool()
{
  for (unsigned int i = 0; i < m_created.size(); i++)
    delete m_created[i];
}

//------------------------------------------------------------------------------
// member function: get_global_pool (static)
//
//! Returns the singleton global pool for the item type, T.
//! This allows items to be shared amongst components throughout the
//! verification environment.
//------------------------------------------------------------------------------

template <typename T>
uvm_object_string_pool<T>* uvm_object_string_pool<T>::get_global_pool()
{
  if (m_global_pool == nullptr)
    m_global_pool = new this_type("global_pool");
  return m_global_pool;
}

//------------------------------------------------------------------------------
// member function: get_global (static)
//
//! Returns the specified item instance from the global item pool.
//------------------------------------------------------------------------------

template <typename T>
T* uvm_object_string_pool<T>::get_global( const std::string& key )
{
  return get_global_pool()->get(key);
}

//------------------------------------------------------------------------------
// member function: get (virtual)
//
//! Returns the object item at the given string \p key.
//!
//! If no item exists by the given \p key, a new item is created for that key
//! and returned. The pool owns the items it creates.
//------------------------------------------------------------------------------

template <typename T>
T* uvm_object_string_pool<T>::get( const std::string& key )
{
  typename uvm_pool<std::string, T*>::pool_type::iterator it = this->pool.find(key);

  if (it == this->pool.end())
  {
    T* item = new T(key);
    m_created.push_back(item);
    it = this->pool.insert(std::make_pair(key, item)).first;
  }

  return it->second;
}

//------------------------------------------------------------------------------
// member function: do_delete (virtual)
//
//! Removes the item with the given string \p key from the pool.
//------------------------------------------------------------------------------

template <typename T>
void uvm_object_string_pool<T>::do_delete( const std::string& key )
{
  if (!this->exists(key))
  {
    uvm_report_warning("POOLDEL",
      "delete: key '" + key + "' doesn't exist");
    return;
  }

  this->pool.erase(key);
}

//------------------------------------------------------------------------------
// member function: create (virtual)
//
// Implementation defined
//------------------------------------------------------------------------------

template <typename T>
uvm_object* uvm_object_string_pool<T>::create( const std::string& name )
{
  return new this_type(name);
}

//------------------------------------------------------------------------------
// member function: get_type_name (virtual)
//
// Implementation defined
//------------------------------------------------------------------------------

template <typename T>
const std::string uvm_object_string_pool<T>::get_type_name() const
{
  return "uvm_obj_str_pool";
}

//------------------------------------------------------------------------------
// member function: do_print (virtual)
//
// Implementation defined
//------------------------------------------------------------------------------

template <typename T>
void uvm_object_string_pool<T>::do_print( const uvm_printer& printer ) const
{
  printer.print_array_header("pool", this->num(), "aa_object_string");

  typename uvm_pool<std::string, T*>::pool_type::const_iterator it;

  for( it = this->pool.begin(); it != this->pool.end(); it++ )
  {
    if (it->second != nullptr)
      printer.print_object("[" + it->first + "]", *(it->second), "[");
  }

  printer.print_array_footer();
}

//------------------------------------------------------------------------------
// Typedef: uvm_event_pool
//
//! A pool of events, indexed by name
//------------------------------------------------------------------------------

typedef uvm_object_string_pool<uvm_event> uvm_event_pool;

} // namespace uvm

#endif // UVM_OBJECT_STRING_POOL_H_
