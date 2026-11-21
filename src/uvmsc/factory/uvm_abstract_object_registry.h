//----------------------------------------------------------------------
//   Copyright 2026 Mayur Kubavat
//   Copyright 2012-2020 NXP B.V.
//   Copyright 2007-2010 Mentor Graphics Corporation
//   Copyright 2007-2010 Cadence Design Systems, Inc.
//   Copyright 2010 Synopsys, Inc.
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

#ifndef UVM_ABSTRACT_OBJECT_REGISTRY_H_
#define UVM_ABSTRACT_OBJECT_REGISTRY_H_

#include <string>
#include <sstream>
#include <type_traits>
#include <systemc>

#include "uvmsc/base/uvm_root.h"
#include "uvmsc/base/uvm_component.h"
#include "uvmsc/base/uvm_coreservice_t.h"
#include "uvmsc/base/uvm_default_coreservice_t.h"
#include "uvmsc/report/uvm_report_object.h"
#include "uvmsc/factory/uvm_factory.h"
#include "uvmsc/factory/uvm_object_wrapper.h"
#include "uvmsc/base/uvm_globals.h"

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_abstract_object_registry<T>
//
//! Lightweight factory proxy for an abstract #uvm_object subtype T.
//! Unlike #uvm_object_registry<T>, this proxy never instantiates T via
//! \c new T(...) and returns nullptr from #create_object. Concrete
//! overrides registered against this proxy are returned by #create.
//----------------------------------------------------------------------

template <typename T = uvm_object>
class uvm_abstract_object_registry : public uvm_object_wrapper
{
 public:

  //--------------------------------------------------------------------------
  // UVM Standard LRM API below
  //--------------------------------------------------------------------------

  virtual uvm_object* create_object( const std::string& name = "" );

  virtual const std::string get_type_name() const;

  static uvm_abstract_object_registry<T>* get();

  static T* create( const std::string& name = "",
                    uvm_component* parent = nullptr,
                    const std::string& contxt = "" );

  static void set_type_override( uvm_object_wrapper* override_type,
                                 bool replace = true );

  static void set_inst_override( uvm_object_wrapper* override_type,
                                 const std::string& inst_path,
                                 uvm_component* parent = nullptr );

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  static void destroy( T* obj );

  virtual ~uvm_abstract_object_registry();

 private:
  explicit uvm_abstract_object_registry( const std::string& name = "" );

  template <typename U>
  static auto m_has_register_type_name( int )
    -> decltype(U::m_register_type_name(), std::true_type());

  template <typename U>
  static std::false_type m_has_register_type_name( ... );

  static const std::string m_type_name_prop_impl( std::true_type );
  static const std::string m_type_name_prop_impl( std::false_type );
  static const std::string m_type_name_prop();

  static uvm_abstract_object_registry<T>* me;

}; // class uvm_abstract_object_registry


//----------------------------------------------------------------------
// definition of static members outside class definition
//----------------------------------------------------------------------

template <typename T>
uvm_abstract_object_registry<T>* uvm_abstract_object_registry<T>::me = get();


//----------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------

template <typename T>
uvm_abstract_object_registry<T>::uvm_abstract_object_registry( const std::string& name )
: uvm_object_wrapper(name)
{}

//----------------------------------------------------------------------
// member function: create_object
//----------------------------------------------------------------------

template <typename T>
uvm_object* uvm_abstract_object_registry<T>::create_object( const std::string& name )
{
  (void)name;
  return nullptr;
}

//----------------------------------------------------------------------
// member function: get_type_name
//----------------------------------------------------------------------

template <typename T>
const std::string uvm_abstract_object_registry<T>::get_type_name() const
{
  return uvm_abstract_object_registry<T>::m_type_name_prop();
}

//----------------------------------------------------------------------
// member function: get (static)
//----------------------------------------------------------------------

template <typename T>
uvm_abstract_object_registry<T>* uvm_abstract_object_registry<T>::get()
{
  if (me == nullptr)
  {
    uvm_coreservice_t* cs = uvm_coreservice_t::get();
    uvm_factory* f = cs->get_factory();

    me = new uvm_abstract_object_registry<T>("absobjrgy_" + m_type_name_prop());
    f->do_register(me);
  }
  return me;
}

//----------------------------------------------------------------------
// member function: create (static)
//----------------------------------------------------------------------

template <typename T>
T* uvm_abstract_object_registry<T>::create( const std::string& name,
                                            uvm_component* parent,
                                            const std::string& contxt )
{
  std::string l_contxt = contxt;
  uvm_object* obj = nullptr;

  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_factory* f = cs->get_factory();

  if (l_contxt.empty() && parent != nullptr)
    l_contxt = parent->get_full_name();

  obj = f->create_object_by_type(get(), l_contxt, name);
  T* robj = dynamic_cast<T*>(obj);
  if (robj == nullptr)
  {
    std::ostringstream msg;
    msg << "Factory did not return an object of type '" << m_type_name_prop() << "'."
        << " A object of type '" << ((obj == nullptr) ? "null" : obj->get_type_name())
        << "' was returned instead. Name=" << name << " Parent="
        << ((parent == nullptr) ? "nullptr" : parent->get_type_name()) << " contxt=" << l_contxt;

    uvm_report_fatal("FCTTYP", msg.str(), UVM_NONE);
  }
  return robj;
}

//----------------------------------------------------------------------
// member function: set_type_override (static)
//----------------------------------------------------------------------

template <typename T>
void uvm_abstract_object_registry<T>::set_type_override(
  uvm_object_wrapper* override_type,
  bool replace )
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_factory* factory = cs->get_factory();

  factory->set_type_override_by_type(get(), override_type, replace);
}

//----------------------------------------------------------------------
// member function: set_inst_override (static)
//----------------------------------------------------------------------

template <typename T>
void uvm_abstract_object_registry<T>::set_inst_override(
  uvm_object_wrapper* override_type,
  const std::string& inst_path,
  uvm_component* parent )
{
  std::ostringstream loc_inst_path;
  if (parent != nullptr)
  {
    if (inst_path.empty())
      loc_inst_path << parent->get_full_name();
    else
      loc_inst_path << parent->get_full_name() << "." << inst_path;
  }
  else
  {
    loc_inst_path << inst_path;
  }
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_factory* factory = cs->get_factory();

  factory->set_inst_override_by_type(get(), override_type, loc_inst_path.str());
}

//----------------------------------------------------------------------
// member function: m_type_name_prop (static)
//----------------------------------------------------------------------

template <typename T>
const std::string uvm_abstract_object_registry<T>::m_type_name_prop_impl( std::true_type )
{
  return T::m_register_type_name();
}

template <typename T>
const std::string uvm_abstract_object_registry<T>::m_type_name_prop_impl( std::false_type )
{
  return "<unknown>";
}

template <typename T>
const std::string uvm_abstract_object_registry<T>::m_type_name_prop()
{
  return m_type_name_prop_impl(decltype(m_has_register_type_name<T>(0))());
}

//----------------------------------------------------------------------
// member function: destroy (static)
//----------------------------------------------------------------------

template <typename T>
void uvm_abstract_object_registry<T>::destroy( T* obj )
{
  if (obj == nullptr)
    return;

  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_factory* f = cs->get_factory();

  if (!f->m_delete_object(obj))
  {
    std::ostringstream msg;
    msg << "Could not destroy object of type '" << obj->get_type_name()
        << "', name=" << obj->get_name()
        << " from factory";
    uvm_report_warning("FCTTYP", msg.str(), UVM_NONE);
    return;
  }
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------

template <typename T>
uvm_abstract_object_registry<T>::~uvm_abstract_object_registry()
{
  if (me == this)
    me = nullptr;
}

} // namespace uvm

#endif // UVM_ABSTRACT_OBJECT_REGISTRY_H_
