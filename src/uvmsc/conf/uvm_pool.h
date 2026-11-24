//------------------------------------------------------------------------------
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2010 Cadence Design Systems, Inc.
//   Copyright 2010 Synopsys, Inc.
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
//------------------------------------------------------------------------------

#ifndef UVM_POOL_H_
#define UVM_POOL_H_

#include <map>
#include <sstream>
#include <string>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/print/uvm_printer.h"

namespace uvm {

namespace detail {

// Writes a value of a pool to a stream when its type can be written to
// one, and a placeholder otherwise, so that a pool can hold any type.
template <typename T>
auto uvm_pool_print_value( std::ostream& os, const T& value, int )
  -> decltype( os << value, void() )
{
  os << value;
}

template <typename T>
void uvm_pool_print_value( std::ostream& os, const T&, long )
{
  os << "<value>";
}

} // namespace detail

//------------------------------------------------------------------------------
// CLASS: uvm_pool<KEY,T>
//
//! Implements a class-based dynamic associative array. Allows sparse arrays to
//! be allocated on demand, and passed and stored by reference.
//------------------------------------------------------------------------------

template <typename KEY = int, typename T = int>
class uvm_pool : public uvm_object
{
 public:
  typedef uvm_pool<KEY,T> this_type;

  //--------------------------------------------------------------------------
  // UVM Standard LRM API below
  //--------------------------------------------------------------------------

  explicit uvm_pool( const std::string& name = "" );
  virtual ~uvm_pool();

  static this_type* get_global_pool();
  static T get_global( const KEY& key );

  virtual T get( const KEY& key );
  virtual void add( const KEY& key, const T& item );
  virtual int num() const;
  virtual void do_delete( const KEY& key );
  virtual bool exists( const KEY& key ) const;

  virtual bool first( KEY& key ) const;
  virtual bool last( KEY& key ) const;
  virtual bool next( KEY& key ) const;
  virtual bool prev( KEY& key ) const;

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  virtual uvm_object* create( const std::string& name = "" );

  virtual const std::string get_type_name() const;

  virtual void do_copy( const uvm_object& rhs );

  virtual void do_print( const uvm_printer& printer ) const;

 protected:
  typedef std::map<KEY,T> pool_type;

  pool_type pool;

 private:
  static this_type* m_global_pool;

}; // class uvm_pool


/////////////////////////////////////////
// Class implementation starts here
/////////////////////////////////////////

//------------------------------------------------------------------------------
// Initialization of static data members
//------------------------------------------------------------------------------

template <typename KEY, typename T>
uvm_pool<KEY,T>* uvm_pool<KEY,T>::m_global_pool = nullptr;

//------------------------------------------------------------------------------
// Constructor
//
//! Creates a new pool with the given \p name.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
uvm_pool<KEY,T>::uvm_pool( const std::string& name )
: uvm_object(name)
{}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------

template <typename KEY, typename T>
uvm_pool<KEY,T>::~uvm_pool()
{}

//------------------------------------------------------------------------------
// member function: get_global_pool (static)
//
//! Returns the singleton global pool for the item type, T.
//! This allows items to be shared amongst components throughout the
//! verification environment.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
uvm_pool<KEY,T>* uvm_pool<KEY,T>::get_global_pool()
{
  if (m_global_pool == nullptr)
    m_global_pool = new this_type("pool");
  return m_global_pool;
}

//------------------------------------------------------------------------------
// member function: get_global (static)
//
//! Returns the specified item instance from the global item pool.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
T uvm_pool<KEY,T>::get_global( const KEY& key )
{
  return get_global_pool()->get(key);
}

//------------------------------------------------------------------------------
// member function: get (virtual)
//
//! Returns the item with the given \p key.
//! If no item exists by that key, a new item is created with that key
//! and returned.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
T uvm_pool<KEY,T>::get( const KEY& key )
{
  typename pool_type::iterator it = pool.find(key);

  if (it == pool.end())
    it = pool.insert(std::make_pair(key, T())).first;

  return it->second;
}

//------------------------------------------------------------------------------
// member function: add (virtual)
//
//! Adds the given (\p key, \p item) pair to the pool. If an item already
//! exists at the given key it is overwritten with the new item.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
void uvm_pool<KEY,T>::add( const KEY& key, const T& item )
{
  typename pool_type::iterator it = pool.find(key);

  if (it == pool.end())
    pool.insert(std::make_pair(key, item));
  else
    it->second = item;
}

//------------------------------------------------------------------------------
// member function: num (virtual)
//
//! Returns the number of uniquely keyed items stored in the pool.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
int uvm_pool<KEY,T>::num() const
{
  return (int)pool.size();
}

//------------------------------------------------------------------------------
// member function: do_delete (virtual)
//
//! Removes the item with the given \p key from the pool.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
void uvm_pool<KEY,T>::do_delete( const KEY& key )
{
  if (!exists(key))
  {
    uvm_report_warning("POOLDEL",
      "delete: pool key doesn't exist. Ignoring delete request");
    return;
  }

  pool.erase(key);
}

//------------------------------------------------------------------------------
// member function: exists (virtual)
//
//! Returns true if an item with the given \p key exists in the pool,
//! false otherwise.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
bool uvm_pool<KEY,T>::exists( const KEY& key ) const
{
  return pool.find(key) != pool.end();
}

//------------------------------------------------------------------------------
// member function: first (virtual)
//
//! Returns the key of the first item stored in the pool.
//!
//! If the pool is empty, then \p key is unchanged and false is returned.
//! If the pool is not empty, then \p key is the key of the first item
//! and true is returned.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
bool uvm_pool<KEY,T>::first( KEY& key ) const
{
  if (pool.empty())
    return false;

  key = pool.begin()->first;
  return true;
}

//------------------------------------------------------------------------------
// member function: last (virtual)
//
//! Returns the key of the last item stored in the pool.
//!
//! If the pool is empty, then \p key is unchanged and false is returned.
//! If the pool is not empty, then \p key is the key of the last item
//! and true is returned.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
bool uvm_pool<KEY,T>::last( KEY& key ) const
{
  if (pool.empty())
    return false;

  key = pool.rbegin()->first;
  return true;
}

//------------------------------------------------------------------------------
// member function: next (virtual)
//
//! Returns the key of the next item in the pool.
//!
//! If the input \p key is the last key in the pool, then \p key is
//! left unchanged and false is returned.
//! If a next key is found, then \p key is updated with that key
//! and true is returned.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
bool uvm_pool<KEY,T>::next( KEY& key ) const
{
  typename pool_type::const_iterator it = pool.upper_bound(key);

  if (it == pool.end())
    return false;

  key = it->first;
  return true;
}

//------------------------------------------------------------------------------
// member function: prev (virtual)
//
//! Returns the key of the previous item in the pool.
//!
//! If the input \p key is the first key in the pool, then \p key is
//! left unchanged and false is returned.
//! If a previous key is found, then \p key is updated with that key
//! and true is returned.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
bool uvm_pool<KEY,T>::prev( KEY& key ) const
{
  typename pool_type::const_iterator it = pool.lower_bound(key);

  if (it == pool.begin())
    return false;

  it--;
  key = it->first;
  return true;
}

//------------------------------------------------------------------------------
// member function: create (virtual)
//
// Implementation defined
//------------------------------------------------------------------------------

template <typename KEY, typename T>
uvm_object* uvm_pool<KEY,T>::create( const std::string& name )
{
  return new this_type(name);
}

//------------------------------------------------------------------------------
// member function: get_type_name (virtual)
//
// Implementation defined
//------------------------------------------------------------------------------

template <typename KEY, typename T>
const std::string uvm_pool<KEY,T>::get_type_name() const
{
  return "uvm_pool";
}

//------------------------------------------------------------------------------
// member function: do_copy (virtual)
//
// Implementation defined
// Copies the items. A pool of pointers is copied as in UVM-SV: the copy
// refers to the same objects, and does not own them.
//------------------------------------------------------------------------------

template <typename KEY, typename T>
void uvm_pool<KEY,T>::do_copy( const uvm_object& rhs )
{
  const this_type* p = dynamic_cast<const this_type*>(&rhs);

  uvm_object::do_copy(rhs);

  if (p == nullptr)
    return;

  pool = p->pool;
}

//------------------------------------------------------------------------------
// member function: do_print (virtual)
//
// Implementation defined
//------------------------------------------------------------------------------

template <typename KEY, typename T>
void uvm_pool<KEY,T>::do_print( const uvm_printer& printer ) const
{
  int cnt = 0;

  printer.print_array_header("pool", num(), "aa_object_string");

  for( typename pool_type::const_iterator it = pool.begin(); it != pool.end(); it++ )
  {
    std::ostringstream item, value;
    item << "[-key" << cnt++ << "--]";
    detail::uvm_pool_print_value(value, it->second, 0);
    printer.print_generic(item.str(), "", -1, value.str(), "[");
  }

  printer.print_array_footer();
}

} // namespace uvm

#endif // UVM_POOL_H_
