//----------------------------------------------------------------------
//   Copyright 2013-2014 Fraunhofer-Gesellschaft zur Foerderung
//					der angewandten Forschung e.V.
//   Copyright 2012-2020 NXP B.V.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2011 Cadence Design Systems, Inc.
//   Copyright 2010-2011 Synopsys, Inc.
//   Copyright 2017 Intel Corp.
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

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <list>
#include <set>
#include <vector>
#include <limits>
#include <algorithm>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/base/uvm_component.h"
#include "uvmsc/base/uvm_coreservice_t.h"
#include "uvmsc/base/uvm_default_coreservice_t.h"
#include "uvmsc/factory/uvm_default_factory.h"
#include "uvmsc/factory/uvm_factory.h"
#include "uvmsc/factory/uvm_factory_override.h"
#include "uvmsc/factory/uvm_object_wrapper.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/macros/uvm_string_defines.h"
#include "uvmsc/misc/uvm_misc.h"


namespace uvm {

namespace {

struct m_candidate_override_t
{
  uvm_factory_override* ovr;
  unsigned long long seq;
  size_t order;
};

struct m_candidate_override_cmp
{
  bool operator()( const m_candidate_override_t& a,
                   const m_candidate_override_t& b ) const
  {
    if (a.seq != b.seq)
      return a.seq < b.seq;
    return a.order < b.order;
  }
};

} // anonymous namespace

//----------------------------------------------------------------------------
// uvm_default_factory implementation
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// initialization static member variables
//----------------------------------------------------------------------------

bool uvm_default_factory::m_debug_pass = false;

//----------------------------------------------------------------------------
// constructor uvm_default_factory
//----------------------------------------------------------------------------

uvm_default_factory::uvm_default_factory()
  : m_next_override_seq(0)
{
  m_types.clear();
  m_type_names.clear();
  m_lookup_strs.clear();
  m_type_overrides.clear();
  m_wildcard_inst_overrides.clear();

  m_inst_override_queues.clear();
  m_inst_override_name_queues.clear();
  m_override_info.clear();
  m_no_replace_overrides.clear();
  m_override_seq.clear();
  m_override_req_names.clear();
  m_inst_override_by_req_name.clear();
  m_type_aliases.clear();
  m_type_alias_names.clear();
  m_inst_aliases.clear();
}

//----------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------

uvm_default_factory::~uvm_default_factory()
{
  for( m_overrides_listItT
       it = m_type_overrides.begin();
       it != m_type_overrides.end();
       it++ )
    delete *it; // delete uvm_factory_override objects

  for( m_overrides_listItT
       it = m_wildcard_inst_overrides.begin();
       it != m_wildcard_inst_overrides.end();
       it++ )
    delete *it; // delete uvm_factory_override objects

  for( m_overrides_listItT
       it = m_override_info.begin();
       it != m_override_info.end();
       it++ )
    delete *it; // delete uvm_factory_override objects

  for( m_types_mapItT
       it = m_types.begin();
       it != m_types.end();
       it++ )
    delete it->first; // delete uvm_object_wrapper objects

  for( m_type_names_mapItT
       it = m_type_names.begin();
       it != m_type_names.end();
       it++ )
    delete it->second; // delete uvm_object_wrapper objects

  for( m_inst_override_queues_mapItT
       it = m_inst_override_queues.begin();
       it != m_inst_override_queues.end();
       it++ )
  {

    for (m_overrides_listItT
         itq = it->second->queue.begin();
         itq != it->second->queue.end();
         itq++)
      delete *itq; // delete uvm_factory_override objects

    delete it->first;  // delete uvm_object_wrapper objects
    delete it->second; // delete uvm_factory_queue_class objects
  }

  for( m_inst_override_name_queues_mapItT
       it = m_inst_override_name_queues.begin();
       it != m_inst_override_name_queues.end();
       it++ )
  {
    for (m_overrides_listItT
         itq = it->second->queue.begin();
         itq != it->second->queue.end();
         itq++)
      delete *itq; // delete uvm_factory_override objects

    delete it->second; // delete uvm_factory_queue_class objects
  }
}

//----------------------------------------------------------------------------
// member function: do_register
//
//! Registers the given proxy object, \p obj, with the factory. The proxy object
//! is a lightweight substitute for the component or object it represents. When
//! the factory needs to create an object of a given type, it calls the proxy's
//! #create_object or #create_component method to do so.
//!
//! When doing name-based operations, the factory calls the proxy's
//! get_type_name method to match against the \p requested_type_name argument in
//! subsequent calls to #create_component_by_name and #create_object_by_name.
//! If the proxy object's #get_type_name method returns the empty string,
//! name-based lookup is effectively disabled.
//----------------------------------------------------------------------------

void uvm_default_factory::do_register( uvm_object_wrapper* obj )
{
  if (obj == nullptr)
    uvm_report_fatal("NULLWR", "Attempting to register a nullptr object with the factory", UVM_NONE);

  if (!obj->get_type_name().empty() && obj->get_type_name() != "<unknown>")
  {
    if (m_type_names.find(obj->get_type_name()) != m_type_names.end()) // if obj with name exists
    {
      std::ostringstream msg;
      msg << "Type name '" << obj->get_type_name()
          << "' already registered with factory. No string-based lookup "
          << "support for multiple types with the same type name.";
      uvm_report_warning("TPRGED", msg.str(), UVM_NONE);
    }
    else
    {
      m_type_names[obj->get_type_name()] = obj;

      m_type_aliases_mapT::iterator ait = m_type_aliases.find(obj->get_type_name());
      if (ait != m_type_aliases.end())
      {
        if (ait->second != obj)
        {
          std::ostringstream msg;
          msg << "Alias type name '" << obj->get_type_name()
              << "' collides with already registered factory type '"
              << obj->get_type_name() << "'.";
          uvm_report_warning("ALSRGD", msg.str(), UVM_NONE);
        }
        m_type_aliases.erase(ait);
      }

      m_type_alias_names_mapT::iterator anit = m_type_alias_names.find(obj->get_type_name());
      if (anit != m_type_alias_names.end())
      {
        std::ostringstream msg;
        msg << "Alias type name '" << obj->get_type_name()
            << "' collides with already registered factory type '"
            << obj->get_type_name() << "'.";
        uvm_report_warning("ALSRGD", msg.str(), UVM_NONE);
        m_type_alias_names.erase(anit);
      }
    }
  }

  if (m_types.find(obj) != m_types.end()) // if obj exists
  {
    if (!obj->get_type_name().empty() && obj->get_type_name() != "<unknown>")
    {
      std::ostringstream msg;
      msg << "Object type '" << obj->get_type_name()
          << "' already registered with factory. ";
      uvm_report_warning("TPRGED", msg.str(), UVM_NONE);
    }
  }
  else // new object (not in list)
  {
    m_types[obj] = true;

    // If a named override happens before the type is registered, need to copy
    // the override queue and backfill pending type overrides.
    if (!obj->get_type_name().empty() && obj->get_type_name() != "<unknown>")
    {
      m_type_names_mapItT tnit = m_type_names.find(obj->get_type_name());
      if (tnit != m_type_names.end() && tnit->second == obj)
      {
        m_bind_pending_type_overrides(obj->get_type_name(), obj);
        m_migrate_inst_override_name_queue(obj->get_type_name(), obj);
      }
    }

    if( m_wildcard_inst_overrides.size() )
    {
      for( m_overrides_listItT
           it = m_wildcard_inst_overrides.begin();
           it != m_wildcard_inst_overrides.end();
           ++it )
      {
        if(uvm_is_match( (*it)->orig_type_name, obj->get_type_name()))
        {
          if (!check_inst_override_exists(obj, (*it)->ovrd_type, (*it)->full_inst_path))
          {
            if (m_inst_override_queues.find(obj) == m_inst_override_queues.end())
              m_inst_override_queues[obj] = new uvm_factory_queue_class();
            uvm_factory_override* copied =
              new uvm_factory_override((*it)->full_inst_path,
                                       obj->get_type_name(),
                                       obj,
                                       (*it)->ovrd_type);
            m_override_seq[copied] = m_get_override_seq(*it);
            m_inst_override_queues[obj]->queue.push_back(copied);
          }
        }
      }
    }
  }

  m_resolve_pending_aliases();
}

//----------------------------------------------------------------------------
// member function: set_type_override_by_type
//
//! Configures the factory to create an object of the override's type whenever
//! a request is made to create an object of the original type using a context
//! that matches \p full_inst_path. The original type is typically a super class
//! of the override type.
//!
//! When overriding by type, the \p original_type and \p override_type are
//! handles to the types' proxy objects. Preregistration is not required.
//----------------------------------------------------------------------------

void uvm_default_factory::set_type_override_by_type( uvm_object_wrapper* original_type,
                                             uvm_object_wrapper* override_type,
                                             bool replace )
{
  bool replaced = false;

  // check that old and new are not the same
  if (original_type == override_type)
  {
    if (original_type->get_type_name().empty() || original_type->get_type_name() == "<unknown>")
      uvm_report_warning("TYPDUP", "Original and override type arguments are identical", UVM_NONE);
    else
    {
      std::ostringstream msg;
      msg << "Original and override type arguments are identical: "
          << original_type->get_type_name() << ".";
      uvm_report_warning("TYPDUP", msg.str(), UVM_NONE);
    }
    return;
  }

  // register the types if not already done so, for the benefit of string-based lookup
  if ( m_types.find(original_type) == m_types.end() )
    do_register(original_type);

  if ( m_types.find(override_type) == m_types.end() )
    do_register(override_type);

  // check for existing type override
  for( m_overrides_listItT
       it = m_type_overrides.begin();
       it != m_type_overrides.end();
       it++ )
  {
    if ( (*it)->orig_type == original_type ||
        ( (*it)->orig_type_name != "<unknown>" &&
          !(*it)->orig_type_name.empty() &&
          (*it)->orig_type_name == original_type->get_type_name()))
    {
      std::ostringstream msg;
      msg << "Original object type '" << original_type->get_type_name()
          << "' already registered to produce '"
          << (*it)->ovrd_type_name << "'." << std::endl;

      if (!replace)
      {
        msg << "Set 'replace' argument to replace the existing entry.";
        uvm_report_info("TPREGD", msg.str(), UVM_MEDIUM);
        return;
      }
      msg << "Replacing with override to produce type '" << override_type->get_type_name()
          << "'.";

      uvm_report_info("TPREGR", msg.str(), UVM_MEDIUM);
      replaced = true;
      (*it)->orig_type = original_type;
      (*it)->orig_type_name = original_type->get_type_name();
      (*it)->ovrd_type = override_type;
      (*it)->ovrd_type_name = override_type->get_type_name();
      m_no_replace_overrides.erase(*it);
      break;
    }
  }

  // make a new entry
  if (!replaced) {
    uvm_factory_override* override;
    override = new uvm_factory_override( "*",
                                         original_type->get_type_name(),
                                         original_type,
                                         override_type );
    m_type_overrides.push_back(override);
    if (!replace)
      m_no_replace_overrides.insert(override);
  }
}


//----------------------------------------------------------------------------
// member function: set_type_override_by_name
//
//! Configures the factory to create an object of the override's type whenever
//! a request is made to create an object of the original type, provided no
//! instance override applies. The original type is typically a super class of
//! the override type.
//!
//! When overriding by name, the \p original_type_name typically refers to a
//! preregistered type in the factory. It may, however, be any arbitrary string.
//! Future calls to any of the create_* methods with the same string and
//! matching instance path will produce the type represented by
//! override_type_name, which must be preregistered with the factory.
//! When replace is set to true, a previous override on original_type_name is
//! replaced, otherwise a previous override, if any, remains intact.
//----------------------------------------------------------------------------

void uvm_default_factory::set_type_override_by_name( const std::string& original_type_name,
                                             const std::string& override_type_name,
                                             bool replace )
{
  bool replaced = false;
  uvm_object_wrapper* original_type = nullptr;
  uvm_object_wrapper* override_type = nullptr;

  original_type = m_resolve_type_alias(original_type_name);
  override_type = m_resolve_type_alias(override_type_name);

  // check that type is registered with the factory
  if (override_type == nullptr)
  {
    std::ostringstream msg;
    msg << "Cannot register override for original type '" <<  original_type_name
        << "' because the override type '" << override_type_name
        << "' is not registered with the factory.";
    uvm_report_error("TYPNTF", msg.str(), UVM_NONE);
    return;
  }

  // check that old and new are not the same
  if ( original_type_name == override_type_name ||
       (original_type != nullptr && original_type == override_type) )
  {
    std::ostringstream msg;
    msg << "Requested and actual type name arguments are identical: "
        << original_type_name << ". Ignoring this override.";
    uvm_report_warning("TYPDUP", msg.str(), UVM_NONE);
    return;
  }

  for ( m_overrides_listItT
        it = m_type_overrides.begin();
        it != m_type_overrides.end();
        it++ )
  {
    if ( (*it)->orig_type_name == original_type_name ||
         ( original_type != nullptr &&
           ( (*it)->orig_type == original_type ||
             ( (*it)->orig_type == nullptr &&
               !(*it)->orig_type_name.empty() &&
               (*it)->orig_type_name != "<unknown>" &&
               (*it)->orig_type_name == original_type->get_type_name() ) ) ) )
    {
      if (!replace)
      {
        std::ostringstream msg;
        msg << "Original type '" << original_type_name
            << "' already registered to produce '" << (*it)->ovrd_type_name
            << "'.  Set 'replace' argument to replace the existing entry.";
        uvm_report_info("TPREGD", msg.str(), UVM_MEDIUM);
        return;
      }
        std::ostringstream msg;
        msg << "Original object type '" << original_type_name
            << "' already registered to produce '" << (*it)->ovrd_type_name
            << "'.  Replacing with override to produce type '"
            << override_type_name << "'.";
        uvm_report_info("TPREGR", msg.str() , UVM_MEDIUM);

      replaced = true;
      (*it)->ovrd_type = override_type;
      (*it)->ovrd_type_name = override_type_name;
      if (original_type != nullptr)
      {
        (*it)->orig_type = original_type;
        (*it)->orig_type_name = original_type->get_type_name();
      }
      m_no_replace_overrides.erase(*it);
      break;
    } // if
  } // for

  if (original_type == nullptr)
    m_lookup_strs[original_type_name] = true;

  if (!replaced)
  {
    uvm_factory_override* override = nullptr;
    override = new uvm_factory_override( "*",
                                         original_type_name,
                                         original_type,
                                         override_type );

    m_type_overrides.push_back(override);
    if (!replace)
      m_no_replace_overrides.insert(override);

    // MB: also commented out in original SV code
    // m_type_names[original_type_name] = override.ovrd_type;
  }
}



//----------------------------------------------------------------------------
// member function: set_inst_override_by_type
//
//! Configures the factory to create an object of the override's type whenever
//! a request is made to create an object of the original type using a context
//! that matches \p full_inst_path. The original type is typically a super class
//! of the override type.
//!
//! When overriding by type, the \p original_type and \p override_type are
//! handles to the types' proxy objects. Preregistration is not required.
//!
//! The \p full_inst_path is matched against the contentation of
//! '\p parent_inst_path + "." + \p name' provided in future create requests. The
//! \p full_inst_path may include wildcards (* and ?) such that a single
//! instance override can be applied in multiple contexts. A \p full_inst_path
//! of "*" is effectively a type override, as it will match all contexts.
//!
//! When the factory processes instance overrides, the instance queue is
//! processed in order of override registrations, and the first override
//! match prevails. Thus, more specific overrides should be registered
//! first, followed by more general overrides.
//----------------------------------------------------------------------------

void uvm_default_factory::set_inst_override_by_type( uvm_object_wrapper* original_type,
                                             uvm_object_wrapper* override_type,
                                             const std::string& full_inst_path )
{
  uvm_factory_override* override;

  // register the types if not already done so
  if ( m_types.find(original_type) == m_types.end() )
    do_register(original_type);

  if ( m_types.find(override_type) == m_types.end() )
    do_register(override_type);

  if ( check_inst_override_exists(original_type,override_type,full_inst_path) )
    return;

  if( m_inst_override_queues.find(original_type) == m_inst_override_queues.end() )
    m_inst_override_queues[original_type] = new uvm_factory_queue_class();

  override = new uvm_factory_override( full_inst_path,
                                       original_type->get_type_name(),
                                       original_type,
                                       override_type);

  m_override_seq[override] = ++m_next_override_seq;
  m_inst_override_queues[original_type]->queue.push_back(override);
}


//----------------------------------------------------------------------------
// member function: set_inst_override_by_name
//
//! Configures the factory to create an object of the override's type whenever
//! a request is made to create an object of the original type using a context
//! that matches \p full_inst_path. The original type is typically a super class
//! of the override type.
//!
//! When overriding by name, the \p original_type_name typically refers to a
//! preregistered type in the factory. It may, however, be any arbitrary
//! string. Future calls to any of the create_* methods with the same string
//! and matching instance path will produce the type represented by
//! \p override_type_name, which must be preregistered with the factory.
//
//! The \p full_inst_path is matched against the contentation of
//! '\p parent_inst_path + "." + \p name' provided in future create requests. The
//! \p full_inst_path may include wildcards (* and ?) such that a single
//! instance override can be applied in multiple contexts. A \p full_inst_path
//! of "*" is effectively a type override, as it will match all contexts.
//
//! When the factory processes instance overrides, the instance queue is
//! processed in order of override registrations, and the first override
//! match prevails. Thus, more specific overrides should be registered
//! first, followed by more general overrides.
//----------------------------------------------------------------------------

void uvm_default_factory::set_inst_override_by_name( const std::string& original_type_name,
                                             const std::string& override_type_name,
                                             const std::string& full_inst_path )
{
  uvm_object_wrapper* override_type = m_resolve_type_alias(override_type_name);

  // check that type is registered with the factory
  if (override_type == nullptr)
  {
    std::ostringstream msg;
    msg << "Cannot register instance override with type name '"
        << original_type_name << "' and instance path '"
        << full_inst_path << "' because the type it's supposed to produce, '"
        << override_type_name << "', is not registered with the factory.";
    uvm_report_error("TYPNTF", msg.str(), UVM_NONE);
    return;
  }

  m_set_inst_override_by_name_impl(
    original_type_name, override_type_name, full_inst_path, ++m_next_override_seq);
}

void uvm_default_factory::m_set_inst_override_by_name_impl( const std::string& original_type_name,
                                                            const std::string& override_type_name,
                                                            const std::string& full_inst_path,
                                                            unsigned long long seq )
{
  uvm_object_wrapper* original_type = nullptr;
  uvm_object_wrapper* override_type = nullptr;
  uvm_factory_override* override = nullptr;

  original_type = m_resolve_type_name(original_type_name, full_inst_path);
  override_type = m_resolve_type_alias(override_type_name);

  if (override_type == nullptr)
    return;

  // Propagate wider instance overrides to any instance-aliased targets whose
  // alias path is covered by full_inst_path.
  m_inst_aliases_mapT::const_iterator iit = m_inst_aliases.find(original_type_name);
  if (iit != m_inst_aliases.end())
  {
    for (m_inst_aliases_listT::const_iterator it = iit->second.begin();
         it != iit->second.end(); ++it)
    {
      if (uvm_is_match(full_inst_path, it->full_inst_path))
      {
        uvm_object_wrapper* alt_type = (it->orig_type != nullptr)
          ? it->orig_type
          : m_resolve_type_name(it->orig_type_name, it->full_inst_path);
        if (alt_type != nullptr && alt_type != original_type)
        {
          if (!check_inst_override_exists(alt_type, override_type, it->full_inst_path))
          {
            if (m_inst_override_queues.find(alt_type) == m_inst_override_queues.end())
              m_inst_override_queues[alt_type] = new uvm_factory_queue_class();
            uvm_factory_override* alt_ovr =
              new uvm_factory_override(it->full_inst_path,
                                       alt_type->get_type_name(),
                                       alt_type,
                                       override_type);
            m_override_seq[alt_ovr] = seq;
            m_inst_override_queues[alt_type]->queue.push_back(alt_ovr);
          }
          else
          {
            uvm_factory_override* existing =
              m_find_inst_override(alt_type, override_type, it->full_inst_path);
            m_update_min_override_seq(existing, seq);
          }
        }
      }
    }
  }

  if (original_type == nullptr)
      m_lookup_strs[original_type_name] = true;

  override = new uvm_factory_override( full_inst_path,
                                       original_type_name,
                                       original_type,
                                       override_type);
  m_override_seq[override] = seq;

  if (original_type != nullptr)
  {
    uvm_factory_override* existing = m_find_inst_override(original_type, override_type, full_inst_path);
    if (existing != nullptr)
    {
      bool already_same_req = (original_type_name == original_type->get_type_name());
      if (!already_same_req)
      {
        std::map<const uvm_factory_override*, std::set<std::string> >::const_iterator rnit =
          m_override_req_names.find(existing);
        if (rnit != m_override_req_names.end() &&
            rnit->second.find(original_type_name) != rnit->second.end())
          already_same_req = true;
      }

      if (already_same_req ||
          (seq <= m_get_override_seq(existing) &&
           m_override_req_names.find(existing) == m_override_req_names.end()))
      {
        check_inst_override_exists(original_type, override_type, full_inst_path);
        m_register_inst_override_req_name(existing, original_type_name);
        if (seq < m_get_override_seq(existing) &&
            m_override_req_names.find(existing) == m_override_req_names.end())
          m_override_seq[existing] = seq;
        m_delete_override(override);
        return;
      }

      if (m_inst_override_name_queues.find(original_type_name) == m_inst_override_name_queues.end())
        m_inst_override_name_queues[original_type_name] = new uvm_factory_queue_class();
      override->orig_type = nullptr;
      override->orig_type_name = original_type_name;
      m_inst_override_name_queues[original_type_name]->queue.push_back(override);
      m_register_inst_override_req_name(override, original_type_name);
      return;
    }

    if( m_inst_override_queues.find(original_type) == m_inst_override_queues.end() )
      m_inst_override_queues[original_type] = new uvm_factory_queue_class();
    m_inst_override_queues[original_type]->queue.push_back(override);
    m_register_inst_override_req_name(override, original_type_name);
  }
  else
  {
    if (m_has_wildcard(original_type_name))
    {
      std::vector<std::string> matches;
      for( m_type_names_mapItT
           it = m_type_names.begin();
           it != m_type_names.end();
           it++ )
      {
        const std::string& s = it->first;
        if (!m_has_wildcard(s) && uvm_is_match(original_type_name, s))
          matches.push_back(s);
      }
      for (m_type_aliases_mapT::const_iterator ait = m_type_aliases.begin();
           ait != m_type_aliases.end(); ++ait)
      {
        const std::string& s = ait->first;
        if (!m_has_wildcard(s) && uvm_is_match(original_type_name, s))
          matches.push_back(s);
      }
      for (m_type_alias_names_mapT::const_iterator anit = m_type_alias_names.begin();
           anit != m_type_alias_names.end(); ++anit)
      {
        const std::string& s = anit->first;
        if (!m_has_wildcard(s) && uvm_is_match(original_type_name, s))
          matches.push_back(s);
      }
      for (m_inst_aliases_mapT::const_iterator imit = m_inst_aliases.begin();
           imit != m_inst_aliases.end(); ++imit)
      {
        const std::string& s = imit->first;
        if (!m_has_wildcard(s) && uvm_is_match(original_type_name, s))
          matches.push_back(s);
      }
      std::sort(matches.begin(), matches.end());
      matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
      for (std::vector<std::string>::const_iterator mit = matches.begin();
           mit != matches.end(); ++mit)
      {
        m_set_inst_override_by_name_impl(*mit, override_type_name, full_inst_path, seq);
      }
      m_wildcard_inst_overrides.push_back(override);
    }
    else
    {
      if( m_inst_override_name_queues.find(original_type_name) == m_inst_override_name_queues.end() )
        m_inst_override_name_queues[original_type_name] = new uvm_factory_queue_class();
      m_inst_override_name_queues[original_type_name]->queue.push_back(override);
      m_register_inst_override_req_name(override, original_type_name);
    }
  }
}

//----------------------------------------------------------------------------
// Group: Type & Instance Aliases
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// member function: set_type_alias
//----------------------------------------------------------------------------

void uvm_default_factory::set_type_alias( const std::string& alias_type_name,
                                          uvm_object_wrapper* original_type )
{
  if (alias_type_name.empty() || alias_type_name == "<unknown>")
  {
    uvm_report_warning("ALSNM",
      "Cannot register type alias with empty or '<unknown>' alias_type_name.", UVM_NONE);
    return;
  }

  if (original_type == nullptr)
  {
    uvm_report_error("NULLWR",
      "Attempting to register type alias '" + alias_type_name + "' for nullptr original_type.", UVM_NONE);
    return;
  }

  if (m_types.find(original_type) == m_types.end())
    do_register(original_type);

  m_type_names_mapItT existing = m_type_names.find(alias_type_name);
  if (existing != m_type_names.end())
  {
    if (existing->second != original_type)
    {
      std::ostringstream msg;
      msg << "Alias type name '" << alias_type_name
          << "' collides with already registered factory type '"
          << existing->second->get_type_name() << "'.";
      uvm_report_warning("ALSRGD", msg.str(), UVM_NONE);
    }
    return;
  }

  m_type_alias_names.erase(alias_type_name);
  m_type_aliases[alias_type_name] = original_type;

  m_bind_pending_type_overrides(alias_type_name, original_type);
  m_migrate_inst_override_name_queue(alias_type_name, original_type);

  m_resolve_pending_aliases();
}

void uvm_default_factory::set_type_alias( const std::string& alias_type_name,
                                          const std::string& original_type_name )
{
  if (alias_type_name.empty() || alias_type_name == "<unknown>" ||
      original_type_name.empty() || original_type_name == "<unknown>")
  {
    uvm_report_warning("ALSNM",
      "Cannot register type alias with empty or '<unknown>' type name.", UVM_NONE);
    return;
  }

  if (alias_type_name == original_type_name)
  {
    uvm_report_warning("TYPDUP",
      "Alias and original type name arguments are identical: " + alias_type_name, UVM_NONE);
    return;
  }

  uvm_object_wrapper* orig = m_resolve_type_alias(original_type_name);
  if (orig != nullptr)
  {
    set_type_alias(alias_type_name, orig);
    return;
  }

  m_type_names_mapItT existing = m_type_names.find(alias_type_name);
  if (existing != m_type_names.end())
  {
    std::ostringstream msg;
    msg << "Alias type name '" << alias_type_name
        << "' collides with already registered factory type '"
        << existing->second->get_type_name() << "'.";
    uvm_report_warning("ALSRGD", msg.str(), UVM_NONE);
    return;
  }

  m_type_aliases.erase(alias_type_name);
  m_type_alias_names[alias_type_name] = original_type_name;
  m_resolve_pending_aliases();
}

//----------------------------------------------------------------------------
// member function: set_inst_alias
//----------------------------------------------------------------------------

void uvm_default_factory::set_inst_alias( const std::string& alias_type_name,
                                          uvm_object_wrapper* original_type,
                                          const std::string& full_inst_path )
{
  if (alias_type_name.empty() || alias_type_name == "<unknown>")
  {
    uvm_report_warning("ALSNM",
      "Cannot register instance alias with empty or '<unknown>' alias_type_name.", UVM_NONE);
    return;
  }

  if (original_type == nullptr)
  {
    uvm_report_error("NULLWR",
      "Attempting to register instance alias '" + alias_type_name + "' for nullptr original_type.", UVM_NONE);
    return;
  }

  if (m_types.find(original_type) == m_types.end())
    do_register(original_type);

  m_inst_aliases_listT& entries = m_inst_aliases[alias_type_name];
  for (m_inst_aliases_listT::const_iterator it = entries.begin();
       it != entries.end(); ++it)
  {
    if (it->orig_type == original_type && it->full_inst_path == full_inst_path)
      return;
  }

  m_inst_alias_entry entry;
  entry.alias_type_name = alias_type_name;
  entry.orig_type = original_type;
  entry.orig_type_name = original_type->get_type_name();
  entry.full_inst_path = full_inst_path;
  entries.push_back(entry);

  m_propagate_inst_alias_overrides(alias_type_name, original_type, full_inst_path);
}

void uvm_default_factory::set_inst_alias( const std::string& alias_type_name,
                                          const std::string& original_type_name,
                                          const std::string& full_inst_path )
{
  if (alias_type_name.empty() || alias_type_name == "<unknown>" ||
      original_type_name.empty() || original_type_name == "<unknown>")
  {
    uvm_report_warning("ALSNM",
      "Cannot register instance alias with empty or '<unknown>' type name.", UVM_NONE);
    return;
  }

  if (m_inst_aliases.find(original_type_name) == m_inst_aliases.end())
  {
    uvm_object_wrapper* orig = m_resolve_type_alias(original_type_name);
    if (orig != nullptr)
    {
      set_inst_alias(alias_type_name, orig, full_inst_path);
      return;
    }
  }

  m_inst_aliases_listT& entries = m_inst_aliases[alias_type_name];
  for (m_inst_aliases_listT::const_iterator it = entries.begin();
       it != entries.end(); ++it)
  {
    if (it->orig_type == nullptr &&
        it->orig_type_name == original_type_name &&
        it->full_inst_path == full_inst_path)
      return;
  }

  m_inst_alias_entry entry;
  entry.alias_type_name = alias_type_name;
  entry.orig_type = nullptr;
  entry.orig_type_name = original_type_name;
  entry.full_inst_path = full_inst_path;
  entries.push_back(entry);
}

//----------------------------------------------------------------------------
// member function: m_resolve_type_alias
//----------------------------------------------------------------------------

uvm_object_wrapper* uvm_default_factory::m_resolve_type_alias( const std::string& type_name ) const
{
  m_type_names_mapT::const_iterator nit = m_type_names.find(type_name);
  if (nit != m_type_names.end())
    return nit->second;

  m_type_aliases_mapT::const_iterator ait = m_type_aliases.find(type_name);
  if (ait != m_type_aliases.end())
    return ait->second;

  std::set<std::string> visited;
  std::string cur = type_name;
  while (visited.insert(cur).second)
  {
    m_type_names_mapT::const_iterator cn = m_type_names.find(cur);
    if (cn != m_type_names.end())
      return cn->second;

    m_type_aliases_mapT::const_iterator ca = m_type_aliases.find(cur);
    if (ca != m_type_aliases.end())
      return ca->second;

    m_type_alias_names_mapT::const_iterator next = m_type_alias_names.find(cur);
    if (next == m_type_alias_names.end())
      break;
    cur = next->second;
  }

  return nullptr;
}

//----------------------------------------------------------------------------
// member function: m_resolve_type_name
//----------------------------------------------------------------------------

uvm_object_wrapper* uvm_default_factory::m_resolve_type_name( const std::string& type_name,
                                                              const std::string& full_inst_path ) const
{
  std::set<std::string> visited;
  return m_resolve_type_name_impl(type_name, full_inst_path, visited);
}

uvm_object_wrapper* uvm_default_factory::m_resolve_type_name_impl( const std::string& type_name,
                                                                   const std::string& full_inst_path,
                                                                   std::set<std::string>& visited,
                                                                   std::vector<std::string>* req_chain ) const
{
  if (!visited.insert(type_name).second)
    return nullptr;

  if (req_chain != nullptr)
    req_chain->push_back(type_name);

  m_inst_aliases_mapT::const_iterator iit = m_inst_aliases.find(type_name);
  if (iit != m_inst_aliases.end())
  {
    for (m_inst_aliases_listT::const_iterator it = iit->second.begin();
         it != iit->second.end(); ++it)
    {
      if (uvm_is_match(it->full_inst_path, full_inst_path))
      {
        if (it->orig_type != nullptr)
          return it->orig_type;
        size_t saved_size = (req_chain != nullptr) ? req_chain->size() : 0;
        uvm_object_wrapper* resolved =
          m_resolve_type_name_impl(it->orig_type_name, full_inst_path, visited, req_chain);
        if (resolved != nullptr)
          return resolved;
        if (req_chain != nullptr)
          req_chain->resize(saved_size);
      }
    }
  }

  return m_resolve_type_alias(type_name);
}

//----------------------------------------------------------------------------
// member function: m_bind_pending_type_overrides
//----------------------------------------------------------------------------

void uvm_default_factory::m_bind_pending_type_overrides( const std::string& match_name,
                                                         uvm_object_wrapper* resolved )
{
  if (resolved == nullptr || match_name.empty() || match_name == "<unknown>")
    return;

  const std::string resolved_name = resolved->get_type_name();
  const bool has_resolved_name = (!resolved_name.empty() && resolved_name != "<unknown>");

  uvm_factory_override* canonical = nullptr;
  for (m_overrides_listItT it = m_type_overrides.begin();
       it != m_type_overrides.end(); )
  {
    if ((*it)->orig_type == nullptr && (*it)->orig_type_name == match_name)
    {
      if ((*it)->ovrd_type == resolved)
      {
        std::ostringstream msg;
        msg << "Requested and actual type arguments are identical: "
            << resolved_name << ". Ignoring this override.";
        uvm_report_warning("TYPDUP", msg.str(), UVM_NONE);
        uvm_factory_override* self_ovr = *it;
        it = m_type_overrides.erase(it);
        m_delete_override(self_ovr);
        continue;
      }
      (*it)->orig_type = resolved;
      (*it)->orig_type_name = resolved_name;
    }

    const bool matches_resolved =
      ((*it)->orig_type == resolved) ||
      ((*it)->orig_type == nullptr && has_resolved_name && (*it)->orig_type_name == resolved_name);

    if (matches_resolved)
    {
      if (canonical == nullptr)
      {
        canonical = *it;
        canonical->orig_type = resolved;
        canonical->orig_type_name = resolved_name;
        ++it;
      }
      else
      {
        uvm_factory_override* dup = *it;
        if (m_no_replace_overrides.find(dup) == m_no_replace_overrides.end())
        {
          canonical->ovrd_type = dup->ovrd_type;
          canonical->ovrd_type_name = dup->ovrd_type_name;
          m_no_replace_overrides.erase(canonical);
        }
        it = m_type_overrides.erase(it);
        m_delete_override(dup);
      }
    }
    else
    {
      ++it;
    }
  }
}

//----------------------------------------------------------------------------
// member function: m_migrate_inst_override_name_queue
//----------------------------------------------------------------------------

void uvm_default_factory::m_migrate_inst_override_name_queue( const std::string& match_name,
                                                              uvm_object_wrapper* default_target )
{
  if (default_target == nullptr || match_name.empty() || match_name == "<unknown>")
    return;

  m_inst_override_name_queues_mapItT nit = m_inst_override_name_queues.find(match_name);
  if (nit == m_inst_override_name_queues.end())
    return;

  m_inst_aliases_mapT::const_iterator iit = m_inst_aliases.find(match_name);

  for (m_overrides_listItT qit = nit->second->queue.begin();
       qit != nit->second->queue.end(); )
  {
    uvm_factory_override* ovr = *qit;
    uvm_object_wrapper* target = m_resolve_type_name(match_name, ovr->full_inst_path);

    bool covered_by_unresolved_inst_alias = false;
    if (iit != m_inst_aliases.end())
    {
      for (m_inst_aliases_listT::const_iterator ait = iit->second.begin();
           ait != iit->second.end(); ++ait)
      {
        if (uvm_is_match(ait->full_inst_path, ovr->full_inst_path))
        {
          uvm_object_wrapper* resolved_ait = (ait->orig_type != nullptr)
            ? ait->orig_type
            : m_resolve_type_name(ait->orig_type_name, ovr->full_inst_path);
          if (resolved_ait == nullptr)
          {
            covered_by_unresolved_inst_alias = true;
            break;
          }
        }
      }
    }

    if (covered_by_unresolved_inst_alias)
    {
      ++qit;
      continue;
    }

    if (target == nullptr)
      target = default_target;

    if (iit != m_inst_aliases.end())
    {
      for (m_inst_aliases_listT::const_iterator ait = iit->second.begin();
           ait != iit->second.end(); ++ait)
      {
        if (uvm_is_match(ovr->full_inst_path, ait->full_inst_path))
        {
          uvm_object_wrapper* alt_type = (ait->orig_type != nullptr)
            ? ait->orig_type
            : m_resolve_type_name(ait->orig_type_name, ait->full_inst_path);
          if (alt_type != nullptr && alt_type != target)
          {
            uvm_factory_override* existing =
              m_find_inst_override(alt_type, ovr->ovrd_type, ait->full_inst_path);
            if (existing == nullptr)
            {
              if (m_inst_override_queues.find(alt_type) == m_inst_override_queues.end())
                m_inst_override_queues[alt_type] = new uvm_factory_queue_class();
              uvm_factory_override* alt_ovr =
                new uvm_factory_override(ait->full_inst_path,
                                         alt_type->get_type_name(),
                                         alt_type,
                                         ovr->ovrd_type);
              m_override_seq[alt_ovr] = m_get_override_seq(ovr);
              m_inst_override_queues[alt_type]->queue.push_back(alt_ovr);
            }
            else
            {
              m_update_min_override_seq(existing, m_get_override_seq(ovr));
            }
          }
        }
      }
    }

    qit = nit->second->queue.erase(qit);
    ovr->orig_type = target;
    ovr->orig_type_name = target->get_type_name();
    uvm_factory_override* existing = m_find_inst_override(target, ovr->ovrd_type, ovr->full_inst_path);
    if (existing != nullptr)
    {
      if (m_get_override_seq(ovr) <= m_get_override_seq(existing) &&
          m_override_req_names.find(existing) == m_override_req_names.end())
      {
        check_inst_override_exists(target, ovr->ovrd_type, ovr->full_inst_path);
        m_transfer_override_metadata(ovr, existing);
        m_delete_override(ovr);
      }
      else
      {
        if (m_inst_override_queues.find(target) == m_inst_override_queues.end())
          m_inst_override_queues[target] = new uvm_factory_queue_class();
        m_inst_override_queues[target]->queue.push_back(ovr);
      }
    }
    else
    {
      if (m_inst_override_queues.find(target) == m_inst_override_queues.end())
        m_inst_override_queues[target] = new uvm_factory_queue_class();
      m_inst_override_queues[target]->queue.push_back(ovr);
    }
  }

  if (nit->second->queue.empty())
  {
    delete nit->second;
    m_inst_override_name_queues.erase(nit);
  }
}

//----------------------------------------------------------------------------
// member function: m_propagate_inst_alias_overrides
//----------------------------------------------------------------------------

void uvm_default_factory::m_propagate_inst_alias_overrides( const std::string& alias_type_name,
                                                            uvm_object_wrapper* original_type,
                                                            const std::string& full_inst_path )
{
  if (original_type == nullptr || alias_type_name.empty() || alias_type_name == "<unknown>")
    return;

  m_inst_override_name_queues_mapItT nit = m_inst_override_name_queues.find(alias_type_name);
  if (nit != m_inst_override_name_queues.end())
  {
    for (m_overrides_listItT qit = nit->second->queue.begin();
         qit != nit->second->queue.end(); )
    {
      if (uvm_is_match(full_inst_path, (*qit)->full_inst_path))
      {
        uvm_factory_override* ovr = *qit;
        uvm_factory_override* existing =
          m_find_inst_override(original_type, ovr->ovrd_type, ovr->full_inst_path);
        if (existing != nullptr)
        {
          if (m_get_override_seq(ovr) <= m_get_override_seq(existing) &&
              m_override_req_names.find(existing) == m_override_req_names.end())
          {
            qit = nit->second->queue.erase(qit);
            check_inst_override_exists(original_type, ovr->ovrd_type, ovr->full_inst_path);
            m_transfer_override_metadata(ovr, existing);
            m_delete_override(ovr);
          }
          else
          {
            ++qit;
          }
        }
        else
        {
          qit = nit->second->queue.erase(qit);
          ovr->orig_type = original_type;
          ovr->orig_type_name = original_type->get_type_name();
          if (m_inst_override_queues.find(original_type) == m_inst_override_queues.end())
            m_inst_override_queues[original_type] = new uvm_factory_queue_class();
          m_inst_override_queues[original_type]->queue.push_back(ovr);
        }
      }
      else
      {
        if (uvm_is_match((*qit)->full_inst_path, full_inst_path))
        {
          uvm_factory_override* existing =
            m_find_inst_override(original_type, (*qit)->ovrd_type, full_inst_path);
          if (existing == nullptr)
          {
            if (m_inst_override_queues.find(original_type) == m_inst_override_queues.end())
              m_inst_override_queues[original_type] = new uvm_factory_queue_class();
            uvm_factory_override* alt_ovr =
              new uvm_factory_override(full_inst_path,
                                       original_type->get_type_name(),
                                       original_type,
                                       (*qit)->ovrd_type);
            m_override_seq[alt_ovr] = m_get_override_seq(*qit);
            m_inst_override_queues[original_type]->queue.push_back(alt_ovr);
          }
          else
          {
            m_update_min_override_seq(existing, m_get_override_seq(*qit));
          }
        }
        ++qit;
      }
    }
    if (nit->second->queue.empty())
    {
      delete nit->second;
      m_inst_override_name_queues.erase(nit);
    }
  }

  m_inst_override_by_req_name_mapT::const_iterator bit =
    m_inst_override_by_req_name.find(alias_type_name);
  if (bit != m_inst_override_by_req_name.end())
  {
    for (m_overrides_listT::const_iterator rit = bit->second.begin();
         rit != bit->second.end(); ++rit)
    {
      uvm_factory_override* ovr = *rit;
      if (ovr->orig_type == original_type)
        continue;

      std::string target_path;
      if (uvm_is_match(full_inst_path, ovr->full_inst_path))
        target_path = ovr->full_inst_path;
      else if (uvm_is_match(ovr->full_inst_path, full_inst_path))
        target_path = full_inst_path;

      if (!target_path.empty())
      {
        uvm_factory_override* existing =
          m_find_inst_override(original_type, ovr->ovrd_type, target_path);
        if (existing == nullptr)
        {
          if (m_inst_override_queues.find(original_type) == m_inst_override_queues.end())
            m_inst_override_queues[original_type] = new uvm_factory_queue_class();
          uvm_factory_override* alt_ovr =
            new uvm_factory_override(target_path,
                                     original_type->get_type_name(),
                                     original_type,
                                     ovr->ovrd_type);
          m_override_seq[alt_ovr] = m_get_override_seq(ovr);
          m_inst_override_queues[original_type]->queue.push_back(alt_ovr);
        }
        else
        {
          m_update_min_override_seq(existing, m_get_override_seq(ovr));
        }
      }
    }
  }
}

uvm_factory_override* uvm_default_factory::m_find_inst_override( uvm_object_wrapper* original_type,
                                                                   uvm_object_wrapper* override_type,
                                                                   const std::string& full_inst_path ) const
{
  if (original_type == nullptr)
    return nullptr;

  m_inst_override_queues_mapT::const_iterator qit = m_inst_override_queues.find(original_type);
  if (qit == m_inst_override_queues.end())
    return nullptr;

  const std::string orig_name = original_type->get_type_name();
  for (m_overrides_listT::const_iterator it = qit->second->queue.begin();
       it != qit->second->queue.end(); ++it)
  {
    uvm_factory_override* ovr = *it;
    if (ovr->full_inst_path == full_inst_path &&
        ovr->orig_type == original_type &&
        ovr->ovrd_type == override_type &&
        ovr->orig_type_name == orig_name)
      return ovr;
  }
  return nullptr;
}

void uvm_default_factory::m_register_inst_override_req_name( uvm_factory_override* ovr,
                                                             const std::string& req_name )
{
  if (ovr == nullptr || req_name.empty() || req_name == "<unknown>")
    return;

  if (m_override_req_names[ovr].insert(req_name).second)
    m_inst_override_by_req_name[req_name].push_back(ovr);
}

void uvm_default_factory::m_transfer_override_metadata( uvm_factory_override* from,
                                                        uvm_factory_override* to )
{
  if (from == nullptr || to == nullptr || from == to)
    return;

  unsigned long long from_seq = m_get_override_seq(from);
  unsigned long long to_seq = m_get_override_seq(to);
  if (from_seq < to_seq)
    m_override_seq[to] = from_seq;

  std::map<const uvm_factory_override*, std::set<std::string> >::const_iterator rnit =
    m_override_req_names.find(from);
  if (rnit != m_override_req_names.end())
  {
    for (std::set<std::string>::const_iterator sit = rnit->second.begin();
         sit != rnit->second.end(); ++sit)
      m_register_inst_override_req_name(to, *sit);
  }
}

void uvm_default_factory::m_delete_override( uvm_factory_override* ovr )
{
  if (ovr == nullptr)
    return;

  std::map<const uvm_factory_override*, std::set<std::string> >::iterator rnit =
    m_override_req_names.find(ovr);
  if (rnit != m_override_req_names.end())
  {
    for (std::set<std::string>::const_iterator sit = rnit->second.begin();
         sit != rnit->second.end(); ++sit)
    {
      m_inst_override_by_req_name_mapT::iterator bit = m_inst_override_by_req_name.find(*sit);
      if (bit != m_inst_override_by_req_name.end())
      {
        bit->second.remove(ovr);
        if (bit->second.empty())
          m_inst_override_by_req_name.erase(bit);
      }
    }
    m_override_req_names.erase(rnit);
  }
  m_override_seq.erase(ovr);
  m_no_replace_overrides.erase(ovr);
  delete ovr;
}

void uvm_default_factory::m_update_min_override_seq( uvm_factory_override* ovr,
                                                      unsigned long long seq )
{
  if (ovr == nullptr)
    return;
  if (m_override_req_names.find(ovr) != m_override_req_names.end())
    return;
  if (seq < m_get_override_seq(ovr))
    m_override_seq[ovr] = seq;
}

unsigned long long uvm_default_factory::m_get_override_seq( const uvm_factory_override* ovr ) const
{
  std::map<const uvm_factory_override*, unsigned long long>::const_iterator it =
    m_override_seq.find(ovr);
  if (it != m_override_seq.end())
    return it->second;
  return std::numeric_limits<unsigned long long>::max();
}

//----------------------------------------------------------------------------
// member function: m_resolve_pending_aliases
//----------------------------------------------------------------------------

void uvm_default_factory::m_resolve_pending_aliases()
{
  bool changed = true;
  while (changed && !m_type_alias_names.empty())
  {
    changed = false;
    for (m_type_alias_names_mapT::iterator it = m_type_alias_names.begin();
         it != m_type_alias_names.end(); )
    {
      uvm_object_wrapper* resolved = m_resolve_type_alias(it->second);
      if (resolved != nullptr)
      {
        std::string alias_name = it->first;
        m_type_alias_names.erase(it++);

        m_type_names_mapItT existing = m_type_names.find(alias_name);
        if (existing != m_type_names.end())
        {
          if (existing->second != resolved)
          {
            std::ostringstream msg;
            msg << "Alias type name '" << alias_name
                << "' collides with already registered factory type '"
                << existing->second->get_type_name() << "'.";
            uvm_report_warning("ALSRGD", msg.str(), UVM_NONE);
          }
          continue;
        }

        m_type_aliases[alias_name] = resolved;
        m_bind_pending_type_overrides(alias_name, resolved);
        m_migrate_inst_override_name_queue(alias_name, resolved);

        changed = true;
      }
      else
      {
        ++it;
      }
    }
  }

  for (m_inst_aliases_mapT::iterator mit = m_inst_aliases.begin();
       mit != m_inst_aliases.end(); ++mit)
  {
    for (m_inst_aliases_listT::iterator lit = mit->second.begin();
         lit != mit->second.end(); ++lit)
    {
      if (lit->orig_type == nullptr &&
          m_inst_aliases.find(lit->orig_type_name) == m_inst_aliases.end())
      {
        uvm_object_wrapper* resolved = m_resolve_type_alias(lit->orig_type_name);
        if (resolved != nullptr)
        {
          lit->orig_type = resolved;
          m_propagate_inst_alias_overrides(lit->alias_type_name, resolved, lit->full_inst_path);
        }
      }
    }
  }
}

//----------------------------------------------------------------------------
// Group: Creation
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// member function: create_object_by_type
//----------------------------------------------------------------------------

uvm_object* uvm_default_factory::create_object_by_type( uvm_object_wrapper* requested_type,
                                                const std::string& parent_inst_path,
                                                const std::string& name )
{
  std::string full_inst_path;

  if (parent_inst_path.empty())
    full_inst_path = name;
  else if (!name.empty())
    full_inst_path = parent_inst_path + "." + name;
  else
    full_inst_path = parent_inst_path;

  m_override_info.clear();

  requested_type = find_override_by_type(requested_type, full_inst_path);

  uvm_object* obj = requested_type->create_object(name);

  if (obj != nullptr)
    m_obj_list.push_back(obj); // register object so we can delete after use

  return obj;
}

//----------------------------------------------------------------------------
// member function: create_component_by_type
//----------------------------------------------------------------------------

uvm_component* uvm_default_factory::create_component_by_type( uvm_object_wrapper* requested_type,
                                                      const std::string& parent_inst_path,
                                                      const std::string& name,
                                                      uvm_component* parent )
{
  std::string full_inst_path;

  if (parent_inst_path.empty())
    full_inst_path = name;
  else if (!name.empty())
    full_inst_path = parent_inst_path + "." + name;
  else
    full_inst_path = parent_inst_path;

  m_override_info.clear();

  requested_type = find_override_by_type(requested_type, full_inst_path);
  
  uvm_component* comp = requested_type->create_component(name, parent);

  if (comp != nullptr)
    m_comp_list.push_back(comp); // register comp so we can delete after use

  return comp; 
}

//----------------------------------------------------------------------------
// member function: create_object_by_name
//----------------------------------------------------------------------------

uvm_object* uvm_default_factory::create_object_by_name( const std::string& requested_type_name,
                                                const std::string& parent_inst_path,
                                                const std::string& name )
{
  uvm_object_wrapper* wrapper;
  std::string inst_path;

  if (parent_inst_path.empty())
    inst_path = name;
  else if (!name.empty())
    inst_path = parent_inst_path + "." + name;
  else
    inst_path = parent_inst_path;

  m_override_info.clear();

  wrapper = find_override_by_name(requested_type_name, inst_path);

  // if no override exists, try to use requested_type_name directly
  if (wrapper == nullptr)
  {
    wrapper = m_resolve_type_name(requested_type_name, inst_path);
    if (wrapper == nullptr)
    {
      std::ostringstream msg;
      msg << "Cannot create an object of type '" << requested_type_name
          << "' because it is not registered with the factory.";
      uvm_report_warning("BDTYP", msg.str(), UVM_NONE);
      return nullptr;
    }
  }

  uvm_object* obj = wrapper->create_object(name);

  if (obj != nullptr)
    m_obj_list.push_back(obj); // register object so we can delete after use

  return obj;
}

//----------------------------------------------------------------------------
// member function: create_component_by_name
//----------------------------------------------------------------------------

uvm_component* uvm_default_factory::create_component_by_name( const std::string& requested_type_name,
                                                      const std::string& parent_inst_path,
                                                      const std::string& name,
                                                      uvm_component* parent )
{
  uvm_object_wrapper* wrapper;

  std::string inst_path;

  if (parent_inst_path.empty())
    inst_path = name;
  else if (!name.empty())
    inst_path = parent_inst_path + "." + name;
  else
    inst_path = parent_inst_path;

  m_override_info.clear();

  wrapper = find_override_by_name(requested_type_name, inst_path);

  // if no override exists, try to use requested_type_name directly
  if (wrapper == nullptr)
  {
    wrapper = m_resolve_type_name(requested_type_name, inst_path);
    if (wrapper == nullptr) // not exist
    {
      std::ostringstream msg;
      msg << "Cannot create a component of type '" << requested_type_name
          << "' because it is not registered with the factory.";
      uvm_report_warning("BDTYP", msg.str(), UVM_NONE);
      return nullptr;
    }
  }
  uvm_component* comp = wrapper->create_component(name, parent);

  if (comp != nullptr)
    m_comp_list.push_back(comp); // register comp so we can delete after use

  return comp;
}

//----------------------------------------------------------------------------
// member function: is_type_name_registered
//
//! Returns true if the object with type type_name can be found in the factory.
//! Otherwise it returns false.
//----------------------------------------------------------------------------

bool uvm_default_factory::is_type_name_registered( const std::string& type_name ) const
{
  return (m_resolve_type_alias(type_name) != nullptr); // exists
}

//----------------------------------------------------------------------------
// member function: is_type_registered
//
//! Returns true if the object is registered in the factory.
//! Otherwise it returns false.
//----------------------------------------------------------------------------

bool uvm_default_factory::is_type_registered( uvm_object_wrapper* obj ) const
{
  return(m_types.find(obj) != m_types.end()); // exists
}


//----------------------------------------------------------------------------
// Group: Debug
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
// member function: debug_create_by_type
//
//! The member function #debug_create_by_type shall perform the same search
//! algorithm as the #create_object_by_type method, but it does not create a new
//! object. Instead, it will provide detailed information about what type of
//! object it would return, listing each override that was applied to arrive at
//! the result. Interpretation of the arguments is the same as with the
//! member function #create_object_by_type.
//----------------------------------------------------------------------------

void uvm_default_factory::debug_create_by_type( uvm_object_wrapper* requested_type,
                                        const std::string& parent_inst_path,
                                        const std::string& name )
{
  m_debug_create("", requested_type, parent_inst_path, name);
}

//----------------------------------------------------------------------------
// member function: debug_create_by_name
//
//! The member function #debug_create_by_name shall perform the same search
//! algorithm as the #create_object_by_name method, but it does not create a new
//! object. Instead, it will provide detailed information about what type of
//! object it would return, listing each override that was applied to arrive at
//! the result. Interpretation of the arguments is the same as with the
//! member function #create_object_by_name.
//----------------------------------------------------------------------------

void uvm_default_factory::debug_create_by_name( const std::string& requested_type_name,
                                        const std::string& parent_inst_path,
                                        const std::string& name )
{
  m_debug_create(requested_type_name, nullptr, parent_inst_path, name);
}

//----------------------------------------------------------------------------
// member function: find_override_by_name
//----------------------------------------------------------------------------

uvm_object_wrapper* uvm_default_factory::find_override_by_name( const std::string& requested_type_name,
                                                        const std::string& full_inst_path)
{
  uvm_object_wrapper* rtype = nullptr;
  uvm_factory_queue_class* qc = nullptr;
  uvm_object_wrapper* override = nullptr;
  bool stop_at_selected = false;
  std::vector<std::string> req_chain;
  std::set<std::string> visited;

  rtype = m_resolve_type_name_impl(requested_type_name, full_inst_path, visited, &req_chain);

/*** TODO - was already commented out in UVM-SV implementation
  if(rtype == null) begin
    if(!requested_type_name.empty()) begin
      uvm_report_warning("TYPNTF", {"Requested type name ",
         requested_type_name, " is not registered with the factory. The instance override to ",
         full_inst_path, " is ignored"}, UVM_NONE);
    end
    m_lookup_strs[requested_type_name] = 1;
    return null;
  end
***/

  if (!full_inst_path.empty())
  {
    std::vector<m_candidate_override_t> candidates;
    std::set<uvm_factory_override*> seen;

    for (std::vector<std::string>::const_iterator nit = req_chain.begin();
         nit != req_chain.end(); ++nit)
    {
      if ( m_inst_override_name_queues.find(*nit) !=
           m_inst_override_name_queues.end() )
      {
        uvm_factory_queue_class* nqc = m_inst_override_name_queues[*nit];
        for( m_overrides_listItT
             qit = nqc->queue.begin();
             qit != nqc->queue.end();
             ++qit )
        {
          if ( (*nit != requested_type_name ||
                uvm_is_match( (*qit)->orig_type_name, requested_type_name)) &&
               uvm_is_match( (*qit)->full_inst_path, full_inst_path) &&
               seen.insert(*qit).second )
          {
            m_candidate_override_t c = { *qit, m_get_override_seq(*qit), candidates.size() };
            candidates.push_back(c);
          }
        }
      }
    }

    if (rtype != nullptr &&
        m_inst_override_queues.find(rtype) != m_inst_override_queues.end() ) // if exists
      qc = m_inst_override_queues[rtype];

    if (qc != nullptr)
    {
      for( m_overrides_listItT
           qit = qc->queue.begin();
           qit != qc->queue.end();
           ++qit )
      {
        if ( ( (*qit)->orig_type == rtype ||
               uvm_is_match( (*qit)->orig_type_name, requested_type_name) ) &&
             uvm_is_match( (*qit)->full_inst_path, full_inst_path) &&
             seen.insert(*qit).second )
        {
          m_candidate_override_t c = { *qit, m_get_override_seq(*qit), candidates.size() };
          candidates.push_back(c);
        }
      }
    }

    for (std::vector<std::string>::const_iterator nit = req_chain.begin();
         nit != req_chain.end(); ++nit)
    {
      m_inst_override_by_req_name_mapT::iterator rit = m_inst_override_by_req_name.find(*nit);
      if (rit != m_inst_override_by_req_name.end())
      {
        for (m_overrides_listItT qit = rit->second.begin();
             qit != rit->second.end(); ++qit)
        {
          if ( uvm_is_match( (*qit)->full_inst_path, full_inst_path) &&
               seen.insert(*qit).second )
          {
            m_candidate_override_t c = { *qit, m_get_override_seq(*qit), candidates.size() };
            candidates.push_back(c);
          }
        }
      }
    }

    for( m_overrides_listItT
         wit = m_wildcard_inst_overrides.begin();
         wit != m_wildcard_inst_overrides.end();
         ++wit )
    {
      if (!uvm_is_match((*wit)->full_inst_path, full_inst_path))
        continue;

      bool name_match = false;
      for (std::vector<std::string>::const_iterator nit = req_chain.begin();
           nit != req_chain.end(); ++nit)
      {
        if (uvm_is_match((*wit)->orig_type_name, *nit))
        {
          name_match = true;
          break;
        }
      }
      if (!name_match && rtype != nullptr)
      {
        const std::string& rname = rtype->get_type_name();
        if (!rname.empty() && rname != "<unknown>" &&
            uvm_is_match((*wit)->orig_type_name, rname))
          name_match = true;
        if (!name_match)
        {
          for (m_type_aliases_mapT::const_iterator ait = m_type_aliases.begin();
               ait != m_type_aliases.end(); ++ait)
          {
            if (ait->second == rtype &&
                uvm_is_match((*wit)->orig_type_name, ait->first))
            {
              name_match = true;
              break;
            }
          }
        }
      }

      if (name_match && seen.insert(*wit).second)
      {
        m_candidate_override_t c = { *wit, m_get_override_seq(*wit), candidates.size() };
        candidates.push_back(c);
      }
    }

    if (!candidates.empty())
    {
      std::sort(candidates.begin(), candidates.end(), m_candidate_override_cmp());
      for (size_t i = 0; i < candidates.size(); ++i)
      {
        uvm_factory_override* cand = candidates[i].ovr;
        bool is_self = (cand->ovrd_type == rtype) ||
                       (!requested_type_name.empty() &&
                        requested_type_name != "<unknown>" &&
                        cand->ovrd_type->get_type_name() == requested_type_name);
        cand->selected = false;
        m_override_info.push_back(cand);

        if (m_debug_pass)
        {
          if (override == nullptr)
          {
            override = cand->ovrd_type;
            cand->selected = true;
            stop_at_selected = is_self;
          }
        }
        else
        {
          if (is_self)
            return cand->ovrd_type;
          else
          {
            std::set<uvm_object_wrapper*> visited_types;
            if (rtype != nullptr)
              visited_types.insert(rtype);
            return m_find_override_by_type_impl( cand->ovrd_type, full_inst_path, visited_types );
          }
        }
      }
    }
  }

  // type override - exact match
  for ( m_overrides_listItT
        it = m_type_overrides.begin();
        it != m_type_overrides.end();
        it++ )
  {
    bool match = false;
    if (rtype == nullptr)
      match = ((*it)->orig_type == nullptr && (*it)->orig_type_name == requested_type_name);
    else
      match = ((*it)->orig_type == rtype ||
               ((*it)->orig_type == nullptr &&
                !(*it)->orig_type_name.empty() &&
                (*it)->orig_type_name != "<unknown>" &&
                (*it)->orig_type_name == rtype->get_type_name()));

    if (match)
    {
      bool is_self = (rtype != nullptr && (*it)->ovrd_type == rtype);
      (*it)->selected = false;
      m_override_info.push_back(*it);

      if (m_debug_pass)
      {
        if (override == nullptr)
        {
          override = (*it)->ovrd_type;
          (*it)->selected = true;
          stop_at_selected = is_self;
        }
      }
      else
      {
        if (is_self)
          return rtype;
        else
        {
          std::set<uvm_object_wrapper*> visited_types;
          if (rtype != nullptr)
            visited_types.insert(rtype);
          return m_find_override_by_type_impl( (*it)->ovrd_type, full_inst_path, visited_types );
        }
      }
    }
  }

  if ( m_debug_pass && override != nullptr )
  {
    if (stop_at_selected)
      return override;
    std::set<uvm_object_wrapper*> visited_types;
    if (rtype != nullptr)
      visited_types.insert(rtype);
    return m_find_override_by_type_impl(override, full_inst_path, visited_types);
  }

  // No override found
  return nullptr;
}

//----------------------------------------------------------------------------
// member function: find_override_by_type
//----------------------------------------------------------------------------

uvm_object_wrapper* uvm_default_factory::find_override_by_type( uvm_object_wrapper* requested_type,
                                                        const std::string& full_inst_path)
{
  std::set<uvm_object_wrapper*> visited_types;
  return m_find_override_by_type_impl(requested_type, full_inst_path, visited_types);
}

uvm_object_wrapper* uvm_default_factory::m_find_override_by_type_impl( uvm_object_wrapper* requested_type,
                                                                       const std::string& full_inst_path,
                                                                       std::set<uvm_object_wrapper*>& visited_types )
{
  uvm_object_wrapper* override = nullptr;
  uvm_factory_queue_class* qc = nullptr;

  if (!visited_types.insert(requested_type).second)
  {
    uvm_report_error("OVRDLOOP", "Recursive loop detected while finding override.", UVM_NONE);

    if (!m_debug_pass)
      debug_create_by_type( requested_type, full_inst_path );

    return requested_type;
  }

  if ( m_inst_override_queues.find(requested_type) != m_inst_override_queues.end() )
    qc = m_inst_override_queues[requested_type];

  // inst override; return first match; takes precedence over type overrides
  if ( !full_inst_path.empty() )
  {
    std::vector<m_candidate_override_t> candidates;
    std::set<uvm_factory_override*> seen;

    if ( qc != nullptr )
    {
      for( m_overrides_listItT
           it = qc->queue.begin();
           it != qc->queue.end();
           it++)
      {
        if ( ( (*it)->orig_type == requested_type ||
             ( (*it)->orig_type_name != "<unknown>" &&
               !(*it)->orig_type_name.empty() &&
               (*it)->orig_type_name == requested_type->get_type_name())) &&
               uvm_is_match( (*it)->full_inst_path, full_inst_path) &&
               seen.insert(*it).second )
        {
          m_candidate_override_t c = { *it, m_get_override_seq(*it), candidates.size() };
          candidates.push_back(c);
        }
      }
    }

    if (!m_inst_aliases.empty() &&
        (!m_inst_override_name_queues.empty() ||
         !m_inst_override_by_req_name.empty() ||
         !m_wildcard_inst_overrides.empty()))
    {
      for (m_inst_aliases_mapT::const_iterator mit = m_inst_aliases.begin();
           mit != m_inst_aliases.end(); ++mit)
      {
        std::set<std::string> visited_names;
        std::vector<std::string> alias_chain;
        if (m_resolve_type_name_impl(mit->first, full_inst_path, visited_names, &alias_chain) == requested_type)
        {
          for (std::vector<std::string>::const_iterator nit = alias_chain.begin();
               nit != alias_chain.end(); ++nit)
          {
            m_inst_override_name_queues_mapItT nqit = m_inst_override_name_queues.find(*nit);
            if (nqit != m_inst_override_name_queues.end())
            {
              for (m_overrides_listItT qit = nqit->second->queue.begin();
                   qit != nqit->second->queue.end(); ++qit)
              {
                if (uvm_is_match((*qit)->full_inst_path, full_inst_path) &&
                    seen.insert(*qit).second)
                {
                  m_candidate_override_t c = { *qit, m_get_override_seq(*qit), candidates.size() };
                  candidates.push_back(c);
                }
              }
            }

            m_inst_override_by_req_name_mapT::iterator rit = m_inst_override_by_req_name.find(*nit);
            if (rit != m_inst_override_by_req_name.end())
            {
              for (m_overrides_listItT qit = rit->second.begin();
                   qit != rit->second.end(); ++qit)
              {
                if (uvm_is_match((*qit)->full_inst_path, full_inst_path) &&
                    seen.insert(*qit).second)
                {
                  m_candidate_override_t c = { *qit, m_get_override_seq(*qit), candidates.size() };
                  candidates.push_back(c);
                }
              }
            }

            for (m_overrides_listItT wit = m_wildcard_inst_overrides.begin();
                 wit != m_wildcard_inst_overrides.end(); ++wit)
            {
              if (uvm_is_match((*wit)->orig_type_name, *nit) &&
                  uvm_is_match((*wit)->full_inst_path, full_inst_path) &&
                  seen.insert(*wit).second)
              {
                m_candidate_override_t c = { *wit, m_get_override_seq(*wit), candidates.size() };
                candidates.push_back(c);
              }
            }
          }
        }
      }
    }

    if (!m_wildcard_inst_overrides.empty() && !m_type_aliases.empty())
    {
      for (m_overrides_listItT wit = m_wildcard_inst_overrides.begin();
           wit != m_wildcard_inst_overrides.end(); ++wit)
      {
        if (!uvm_is_match((*wit)->full_inst_path, full_inst_path))
          continue;
        for (m_type_aliases_mapT::const_iterator ait = m_type_aliases.begin();
             ait != m_type_aliases.end(); ++ait)
        {
          if (ait->second == requested_type &&
              uvm_is_match((*wit)->orig_type_name, ait->first) &&
              seen.insert(*wit).second)
          {
            m_candidate_override_t c = { *wit, m_get_override_seq(*wit), candidates.size() };
            candidates.push_back(c);
            break;
          }
        }
      }
    }

    if (!candidates.empty())
    {
      std::sort(candidates.begin(), candidates.end(), m_candidate_override_cmp());
      for (size_t i = 0; i < candidates.size(); ++i)
      {
        uvm_factory_override* cand = candidates[i].ovr;
        cand->selected = false;
        m_override_info.push_back(cand);

        if (m_debug_pass) {
          if (override == nullptr) {
            override = cand->ovrd_type;
            cand->selected = true;
          }
        }
        else {
          if ( cand->ovrd_type == requested_type)
            return requested_type;
          else
            return m_find_override_by_type_impl( cand->ovrd_type, full_inst_path, visited_types );
        }
      }
    }
  }

  // type override - exact match
  for( m_overrides_listItT
      it = m_type_overrides.begin();
      it != m_type_overrides.end();
      it++ )
  {
    if ( (*it)->orig_type == requested_type ||
      ( (*it)->orig_type_name != "<unknown>" &&
        !(*it)->orig_type_name.empty() &&
        requested_type != nullptr &&
        (*it)->orig_type_name == requested_type->get_type_name()) )
    {
      (*it)->selected = false;
      m_override_info.push_back(*it);

      if (m_debug_pass) {
        if (override == nullptr) {
          override = (*it)->ovrd_type;
          (*it)->selected = true;
        }
      }
      else {
        if ((*it)->ovrd_type == requested_type)
          return requested_type;
        else
          return m_find_override_by_type_impl((*it)->ovrd_type, full_inst_path, visited_types);
      }
    }
  }


  // type override with wildcard match -- TODO: Already commented-out in SV code
  //foreach (m_type_overrides[index])
  //  if (uvm_is_match(index,requested_type.get_type_name())) begin
  //    m_override_info.push_back(m_inst_overrides[index]);
  //    return find_override_by_type(m_type_overrides[index],full_inst_path);
  //  end

  if ( m_debug_pass && override != nullptr )
  {
    if (override == requested_type)
      return requested_type;
    else
      return m_find_override_by_type_impl(override, full_inst_path, visited_types);
  }

  return requested_type;
}

//----------------------------------------------------------------------------
// member function: find_wrapper_by_name
//
//! This member function returns the uvm_object_wrapper associated with a given
//! type_name. If not found, it returns nullptr.
//----------------------------------------------------------------------------

uvm_object_wrapper* uvm_default_factory::find_wrapper_by_name( const std::string& type_name )
{
  uvm_object_wrapper* wrapper = m_resolve_type_alias(type_name);
  if (wrapper != nullptr)
    return wrapper;

  uvm_report_warning("UnknownTypeName", "find_wrapper_by_name: Type name '" + type_name +
      "' not registered with the factory.", UVM_NONE);
  return nullptr;
}

//----------------------------------------------------------------------------
// member function: print
//
//! Prints the state of the uvm_factory, including registered types, instance
//! overrides, and type overrides.
//!
//! When all_types is 0, only type and instance overrides are displayed.
//! When all_types is 1 (default), all registered user-defined types are
//! printed as well, provided they have names associated with them. When
//! all_types is 2, the UVM types (prefixed with uvm_) are included in the
//! list of registered types.
//----------------------------------------------------------------------------

void uvm_default_factory::print( int all_types )
{
  std::string key;
  std::map<std::string, uvm_factory_queue_class*> sorted_override_queues;
  std::string tmp;
  std::vector<std::string> qs;
  int id=0;
  uvm_object_wrapper* obj = nullptr;

  //sort the override queues
  for ( std::map<uvm_object_wrapper*, uvm_factory_queue_class*>::iterator
        it = m_inst_override_queues.begin();
        it != m_inst_override_queues.end();
        it++ )
  {
    obj = (*it).first;
    tmp = obj->get_type_name();
    if (tmp.empty())
    {
      std::ostringstream str;
      str << "__unnamed_id_" << id++;
      tmp = str.str();
    }
    sorted_override_queues[tmp] = (*it).second;
  }

  for( std::map<std::string, uvm_factory_queue_class*>::iterator
       it = m_inst_override_name_queues.begin();
       it != m_inst_override_name_queues.end();
       it++ )
  {
    sorted_override_queues[(*it).first] = m_inst_override_name_queues[(*it).first];
  }

  qs.push_back("\n#### Print Factory Configuration (*)\n\n");

  // print instance overrides
  if( !m_type_overrides.size() && !sorted_override_queues.size() )
    qs.push_back("No instance or type overrides are registered with this factory\n");
  else
  {
    unsigned int max1 = 14;
    unsigned int max2 = 13;
    unsigned int max3 = 13;
    std::string dash = "---------------------------------------------------------------------------------------------------";
    std::string space= "                                                                                                   ";

    // print instance overrides
    if(sorted_override_queues.size() == 0)
      qs.push_back("No instance overrides are registered with this factory\n");
    else
    {
      for( m_inst_override_name_queues_mapItT
           it = sorted_override_queues.begin();
           it != sorted_override_queues.end();
           it++)
      {
        uvm_factory_queue_class* qc = (*it).second;
        for ( m_overrides_listItT
              lit = qc->queue.begin();
              lit != qc->queue.end();
              lit++)
        {
          if ((*lit)->orig_type_name.length() > max1)
            max1 = (unsigned int)(*lit)->orig_type_name.length();
          if ((*lit)->full_inst_path.length() > max2)
            max2 = (unsigned int)(*lit)->full_inst_path.length();
          if ((*lit)->ovrd_type_name.length() > max3)
            max3 = (unsigned int)(*lit)->ovrd_type_name.length();
        }
      }
      if (max1 < 14) max1 = 14;
      if (max2 < 13) max2 = 13;
      if (max3 < 13) max3 = 13;

      qs.push_back("Instance Overrides:\n\n");
      qs.push_back("Requested Type" + space.substr(1,max1-14) +
            "  Override Path" + space.substr(1,max2-13) +
            "  Override Type" + space.substr(1,max3-13) + "\n");

      qs.push_back("  " + dash.substr(1,max1) +
           "  " +  dash.substr(1,max2) +
           "  " + dash.substr(1,max3) + "\n" );

      for( m_inst_override_name_queues_mapItT
           it = sorted_override_queues.begin();
           it != sorted_override_queues.end();
           it++)
      {
        uvm_factory_queue_class* qc = (*it).second;
        for ( m_overrides_listItT
              lit = qc->queue.begin();
              lit != qc->queue.end();
              lit++)
        {
          qs.push_back("  " + (*lit)->orig_type_name +
               space.substr(1, max1 - (*lit)->orig_type_name.length()) +
               "  " + (*lit)->full_inst_path +
               space.substr(1, max2 - (*lit)->full_inst_path.length()) +
               "  " + (*lit)->ovrd_type_name + "\n");

        }
      }
    }

    // print type overrides
    if ( m_type_overrides.size() == 0 )
      qs.push_back("\nNo type overrides are registered with this factory\n");
    else
    {
      // Resize for type overrides
      if (max1 < 14) max1 = 14;
      if (max2 < 13) max2 = 13;
      if (max3 < 13) max3 = 13;

      for( m_overrides_listItT
           it = m_type_overrides.begin();
           it != m_type_overrides.end();
           it++ )
      {
        if ((*it)->orig_type_name.length() > max1)
          max1 = (unsigned int)(*it)->orig_type_name.length();
        if ((*it)->ovrd_type_name.length() > max2)
          max2 = (unsigned int)(*it)->ovrd_type_name.length();
      }
      if (max1 < 14) max1 = 14;
      if (max2 < 13) max2 = 13;

      qs.push_back("Type Overrides:\n\n");
      qs.push_back("  Requested Type" + space.substr(1,max1-14) +
           "  Override Type" + space.substr(1,max2-13) + "\n");

      qs.push_back("  " + dash.substr(1,max1) +
           "  " + dash.substr(1,max2) + "\n");

      for( m_overrides_listItT
                 it = m_type_overrides.begin();
                 it != m_type_overrides.end();
                 it++ )
      {
        qs.push_back("  " + (*it)->orig_type_name +
             space.substr(1, max1 - (*it)->orig_type_name.length()) +
             "  " + (*it)->ovrd_type_name + "\n");
      }
    }
  }

  if (!m_type_aliases.empty() || !m_type_alias_names.empty())
  {
    unsigned int col1 = 10;
    unsigned int col2 = 13;
    for (m_type_aliases_mapT::const_iterator it = m_type_aliases.begin();
         it != m_type_aliases.end(); ++it)
    {
      if (it->first.length() > col1)
        col1 = static_cast<unsigned int>(it->first.length());
      std::string orig_nm = (it->second != nullptr) ? it->second->get_type_name() : "<null>";
      if (orig_nm.length() > col2)
        col2 = static_cast<unsigned int>(orig_nm.length());
    }
    for (m_type_alias_names_mapT::const_iterator it = m_type_alias_names.begin();
         it != m_type_alias_names.end(); ++it)
    {
      if (it->first.length() > col1)
        col1 = static_cast<unsigned int>(it->first.length());
      if (it->second.length() > col2)
        col2 = static_cast<unsigned int>(it->second.length());
    }

    qs.push_back("\nType Aliases:\n\n");
    qs.push_back("  Alias Type" + std::string(col1 - 10, ' ') +
                 "  Original Type" + std::string(col2 - 13, ' ') + "\n");
    qs.push_back("  " + std::string(col1, '-') +
                 "  " + std::string(col2, '-') + "\n");

    for (m_type_aliases_mapT::const_iterator it = m_type_aliases.begin();
         it != m_type_aliases.end(); ++it)
    {
      std::string orig_nm = (it->second != nullptr) ? it->second->get_type_name() : "<null>";
      qs.push_back("  " + it->first + std::string(col1 - it->first.length(), ' ') +
                   "  " + orig_nm + "\n");
    }
    for (m_type_alias_names_mapT::const_iterator it = m_type_alias_names.begin();
         it != m_type_alias_names.end(); ++it)
    {
      qs.push_back("  " + it->first + std::string(col1 - it->first.length(), ' ') +
                   "  " + it->second + "\n");
    }
  }

  if (!m_inst_aliases.empty())
  {
    unsigned int col1 = 10;
    unsigned int col2 = 13;
    unsigned int col3 = 13;
    for (m_inst_aliases_mapT::const_iterator mit = m_inst_aliases.begin();
         mit != m_inst_aliases.end(); ++mit)
    {
      for (m_inst_aliases_listT::const_iterator lit = mit->second.begin();
           lit != mit->second.end(); ++lit)
      {
        if (lit->alias_type_name.length() > col1)
          col1 = static_cast<unsigned int>(lit->alias_type_name.length());
        if (lit->full_inst_path.length() > col2)
          col2 = static_cast<unsigned int>(lit->full_inst_path.length());
        std::string orig_nm = (lit->orig_type != nullptr) ? lit->orig_type->get_type_name() : lit->orig_type_name;
        if (orig_nm.length() > col3)
          col3 = static_cast<unsigned int>(orig_nm.length());
      }
    }

    qs.push_back("\nInstance Aliases:\n\n");
    qs.push_back("  Alias Type" + std::string(col1 - 10, ' ') +
                 "  Instance Path" + std::string(col2 - 13, ' ') +
                 "  Original Type" + std::string(col3 - 13, ' ') + "\n");
    qs.push_back("  " + std::string(col1, '-') +
                 "  " + std::string(col2, '-') +
                 "  " + std::string(col3, '-') + "\n");

    for (m_inst_aliases_mapT::const_iterator mit = m_inst_aliases.begin();
         mit != m_inst_aliases.end(); ++mit)
    {
      for (m_inst_aliases_listT::const_iterator lit = mit->second.begin();
           lit != mit->second.end(); ++lit)
      {
        std::string orig_nm = (lit->orig_type != nullptr) ? lit->orig_type->get_type_name() : lit->orig_type_name;
        qs.push_back("  " + lit->alias_type_name + std::string(col1 - lit->alias_type_name.length(), ' ') +
                     "  " + lit->full_inst_path + std::string(col2 - lit->full_inst_path.length(), ' ') +
                     "  " + orig_nm + "\n");
      }
    }
  }

  // print all registered types, if all_types >= 1
  if (all_types >= 1 && m_type_names.size() > 0 )
  {
    std::ostringstream str;
    str << "\nAll types registered with the factory: " << m_types.size() << " total\n";
    qs.push_back(str.str());

    bool banner = false;

    for( m_type_names_mapItT
         it = m_type_names.begin();
         it != m_type_names.end();
         it++ )
    {
      // filter out uvm_ classes (if all_types<2) and non-types (lookup strings)
      if( !(all_types < 2 && uvm_is_match("uvm_*", (*it).first)) //&& key == (*it).first.get_type_name()
          )
      {
        if (!banner)
        {
          qs.push_back("  Type Name\n");
          qs.push_back("  ---------\n");
          banner = true;
        }
        qs.push_back("  " + (*it).first + "\n");
      }
    }
  }

  qs.push_back("(*) Types with no associated type name will be printed as <unknown>\n\n####\n\n");

  UVM_INFO("UVM/FACTORY/PRINT", UVM_STRING_QUEUE_STREAMING_PACK(qs), UVM_NONE);
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//////// Implementation-defined member functions start here ////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
// member function: check_inst_override_exists
//
//! Implementation-defined member function
//----------------------------------------------------------------------------

bool uvm_default_factory::check_inst_override_exists( uvm_object_wrapper* original_type,
                                              uvm_object_wrapper* override_type,
                                              const std::string& full_inst_path )
{
  uvm_factory_override* override = nullptr;
  uvm_factory_queue_class* qc = nullptr;

  // if exists
  if ( m_inst_override_queues.find(original_type) != m_inst_override_queues.end() )
    qc = m_inst_override_queues[original_type];
  else
    return false;

  m_overrides_listItT it;

  for (it = qc->queue.begin();
       it != qc->queue.end();
       it++)
  {
    override = (*it);

    if (override->full_inst_path == full_inst_path &&
        override->orig_type == original_type &&
        override->ovrd_type == override_type &&
        override->orig_type_name == original_type->get_type_name())
    {
      std::ostringstream msg;
      msg << "Instance override for '" << original_type->get_type_name()
          << "' already exists: override type '"
          << override_type->get_type_name()
          << "' with full_inst_path '" << full_inst_path
          << "'";
      uvm_report_info("DUPOVRD", msg.str(), UVM_HIGH);
      return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------------
// member function: m_has_wildcard
//
//! Implementation-defined member function
//----------------------------------------------------------------------------

bool uvm_default_factory::m_has_wildcard( const std::string& nm )
{
  size_t found;
  found = nm.find("*");
  if ( found != std::string::npos ) return true;
  found = nm.find("?");
  if ( found != std::string::npos ) return true;
  return false;
}

//----------------------------------------------------------------------------
// member function: m_debug_create
//
//! Implementation-defined member function
//----------------------------------------------------------------------------

void uvm_default_factory::m_debug_create( const std::string& requested_type_name,
                                  uvm_object_wrapper* requested_type,
                                  const std::string& parent_inst_path,
                                  const std::string& name )
{
  std::string full_inst_path;
  uvm_object_wrapper* result;
  std::string loc_requested_type_name = requested_type_name;

  if (parent_inst_path.empty())
    full_inst_path = name;
  else if (!name.empty())
    full_inst_path = parent_inst_path + "." + name;
  else
    full_inst_path = parent_inst_path;

  m_override_info.clear();

  if (requested_type == nullptr)
  {
    std::set<std::string> visited;
    std::vector<std::string> req_chain;
    uvm_object_wrapper* resolved =
      m_resolve_type_name_impl(requested_type_name, full_inst_path, visited, &req_chain);
    bool has_matching_wildcard = false;
    if (resolved == nullptr &&
        m_lookup_strs.find(requested_type_name) == m_lookup_strs.end() &&
        !full_inst_path.empty())
    {
      for (m_overrides_listItT wit = m_wildcard_inst_overrides.begin();
           wit != m_wildcard_inst_overrides.end() && !has_matching_wildcard; ++wit)
      {
        if (uvm_is_match((*wit)->full_inst_path, full_inst_path))
        {
          for (std::vector<std::string>::const_iterator nit = req_chain.begin();
               nit != req_chain.end(); ++nit)
          {
            if (uvm_is_match((*wit)->orig_type_name, *nit))
            {
              has_matching_wildcard = true;
              break;
            }
          }
        }
      }
    }
    if (resolved == nullptr &&
        m_lookup_strs.find(requested_type_name) == m_lookup_strs.end() &&
        !has_matching_wildcard)
    {
      uvm_report_warning("Factory Warning", "The factory does not recognize '"
        + requested_type_name + "' as a registered type.", UVM_NONE);
      return;
    }
    m_debug_pass = true;

    result = find_override_by_name(requested_type_name, full_inst_path);
    if (result == nullptr && resolved != nullptr)
      result = resolved;
  }
  else
  {
    m_debug_pass = true;
    if (m_types.find(requested_type) == m_types.end() ) // if not exists
      do_register(requested_type);

    result = find_override_by_type(requested_type, full_inst_path);

    if (requested_type_name.empty())
      loc_requested_type_name = requested_type->get_type_name();
  }

  m_debug_display(loc_requested_type_name, result, full_inst_path);
  m_debug_pass = false;

  for( m_overrides_listItT
       it = m_override_info.begin();
       it != m_override_info.end();
       it++ )
    (*it)->selected = false;
}

//----------------------------------------------------------------------------
// member function: m_debug_display
//
//! Implementation-defined member function
//----------------------------------------------------------------------------

void uvm_default_factory::m_debug_display( const std::string& requested_type_name,
                                   uvm_object_wrapper* result,
                                   const std::string& full_inst_path )
{
  unsigned int max1 = 13;
  unsigned int max2 = 13;
  unsigned int max3 = 13;
  std::string dash = "---------------------------------------------------------------------------------------------------";
  std::string space= "                                                                                                   ";
  std::vector<std::string> qs;

  qs.push_back("\n#### Factory Override Information (*)\n\n");

  std::ostringstream str;
  str << "Given a request for an object of type '" << requested_type_name
      << "' with an instance path of '" << full_inst_path
      << "', the factory encountered";

  if (m_override_info.size() == 0)
    qs.push_back(str.str() + " no relevant overrides.\n");
  else
  {

    qs.push_back(str.str() + " the following relevant overrides." +
      "An 'x' next to a\n match indicates a match that was ignored.\n\n");

    for( m_overrides_listItT
         it = m_override_info.begin();
         it != m_override_info.end();
         it++ )
    {
      if ( (*it)->orig_type_name.length() > max1)
        max1 = (unsigned int)(*it)->orig_type_name.length();

      if ( (*it)->full_inst_path.length() > max2)
        max2 = (unsigned int)(*it)->full_inst_path.length();

      if ( (*it)->ovrd_type_name.length() > max3)
        max3 = (unsigned int)(*it)->ovrd_type_name.length();
    }

    if (max1 < 13) max1 = 13;
    if (max2 < 13) max2 = 13;
    if (max3 < 13) max3 = 13;

    qs.push_back("  Original Type" + space.substr(1, max1-13) +
         "  Instance Path" + space.substr(1, max2-13) +
         "  Override Type" + space.substr(1,max3-13) + "\n");

    qs.push_back("  " + dash.substr(1,max1) +
         "  " + dash.substr(1,max2) +
         "  " + dash.substr(1,max3) );

    for( m_overrides_listItT
         it = m_override_info.begin();
         it != m_override_info.end();
         it++ )
    {
      std::ostringstream str;
      str << ((*it)->selected ? "  " : "x ")
          << (*it)->orig_type_name
          << space.substr(1, max1 - (*it)->orig_type_name.length() )
          << "  " << (*it)->full_inst_path
          << space.substr(1, max2 - (*it)->full_inst_path.length() )
          << "  " << (*it)->ovrd_type_name
          << space.substr(1, max3 - (*it)->ovrd_type_name.length() );

      if ((*it)->full_inst_path == "*")
        qs.push_back( str.str() + "  <type override>");
      else
        qs.push_back("\n");
    }
    qs.push_back("\n");
  }


  qs.push_back("Result:\n\n");
  qs.push_back("  The factory will produce an object of type '" +
       ( result == nullptr ? requested_type_name : result->get_type_name() ) +
       "'\n");

  qs.push_back("\n(*) Types with no associated type name will be printed as <unknown>\n\n####\n\n");

  UVM_INFO("UVM/FACTORY/DUMP", UVM_STRING_QUEUE_STREAMING_PACK(qs), UVM_NONE);
}

//----------------------------------------------------------------------------
// member function: m_delete_object
//
//! Implementation-defined member function
//----------------------------------------------------------------------------

bool uvm_default_factory::m_delete_object( uvm_object* obj )
{
  m_obj_listItT it = std::find(m_obj_list.begin(), m_obj_list.end(), obj);

  if ( it == m_obj_list.end() )
    return false; // id not found, so nothing to delete

  delete *it; // delete object registered in map
  m_obj_list.erase(it); // clear object from list

  return true;
}

//----------------------------------------------------------------------------
// member function: m_delete_all_objects
//
//! Implementation-defined member function
//----------------------------------------------------------------------------
void uvm_default_factory::m_delete_all_objects()
{
  for( m_obj_listItT it = m_obj_list.begin();
     it != m_obj_list.end(); ++it)
  delete *it;

  m_obj_list.clear(); // empty whole list
}

//----------------------------------------------------------------------------
// member function: m_delete_component
//
//! Implementation-defined member function
//----------------------------------------------------------------------------

bool uvm_default_factory::m_delete_component( uvm_component* comp )
{
  m_comp_listItT it = std::find(m_comp_list.begin(), m_comp_list.end(), comp);

  if ( it == m_comp_list.end() )
    return false; // id not found, so nothing to delete

  delete *it; // delete object registered in map
  m_comp_list.erase(it); // clear map entry

  return true;
}

//----------------------------------------------------------------------------
// member function: m_delete_all_components
//
//! Implementation-defined member function
//----------------------------------------------------------------------------
void uvm_default_factory::m_delete_all_components()
{
  for( m_comp_listItT it = m_comp_list.begin();
     it != m_comp_list.end(); ++it)
  delete *it;

  m_comp_list.clear(); // empty whole list
}

////////////////////

//------------------------------------------------------------------------------
// implementation of global functions to create uvm_object/uvm_component,
// and to set up overrides.
//------------------------------------------------------------------------------
// TODO move to command line processor

void uvm_set_type_override(
  const std::string& original_type_name,
  const std::string& override_type_name,
  bool replace )
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_factory* factory = cs->get_factory();
  factory->set_type_override_by_name(
    original_type_name, override_type_name, replace );
}

void uvm_set_inst_override(
  const std::string& original_type_name,
  const std::string& override_type_name,
  const std::string& full_inst_path )
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_factory* factory = cs->get_factory();
  factory->set_inst_override_by_name(
    original_type_name, override_type_name, full_inst_path );
}

/////////////

} // namespace uvm
