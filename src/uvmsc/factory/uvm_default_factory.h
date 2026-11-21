//----------------------------------------------------------------------------
//   Copyright 2012-2020 NXP B.V.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2011 Cadence Design Systems, Inc.
//   Copyright 2010-2011 Synopsys, Inc.
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
//----------------------------------------------------------------------------

#ifndef UVM_DEFAULT_FACTORY_H_
#define UVM_DEFAULT_FACTORY_H_

#include <map>
#include <list>
#include <set>
#include <string>
#include <vector>
#include <iostream>

#include "uvmsc/factory/uvm_factory.h"

//////////////////////

namespace uvm {


// forward class declarations
class uvm_object;
class uvm_component;
class uvm_object_wrapper;
class uvm_factory_override;

//----------------------------------------------------------------------------
// Class: uvm_default_factory
//
//! Default implementation of the UVM factory.
//----------------------------------------------------------------------------

class uvm_default_factory : public uvm_factory
{
  friend class uvm_default_coreservice_t;

public:

  //--------------------------------------------------------------------------
  // Group: Registering Types
  //--------------------------------------------------------------------------

  virtual void do_register( uvm_object_wrapper* obj );

  //--------------------------------------------------------------------------
  // Group: Type & Instance Overrides
  //--------------------------------------------------------------------------

  virtual void set_inst_override_by_type( uvm_object_wrapper* original_type,
                                          uvm_object_wrapper* override_type,
                                          const std::string& full_inst_path );

  virtual void set_inst_override_by_name( const std::string& original_type_name,
                                          const std::string& override_type_name,
                                          const std::string& full_inst_path );

  virtual void set_type_override_by_type( uvm_object_wrapper* original_type,
                                          uvm_object_wrapper* override_type,
                                          bool replace = true );

  virtual void set_type_override_by_name( const std::string& original_type_name,
                                          const std::string& override_type_name,
                                          bool replace = true );

  //--------------------------------------------------------------------------
  // Group: Type & Instance Aliases
  //--------------------------------------------------------------------------

  virtual void set_type_alias( const std::string& alias_type_name,
                               uvm_object_wrapper* original_type );

  virtual void set_inst_alias( const std::string& alias_type_name,
                               uvm_object_wrapper* original_type,
                               const std::string& full_inst_path );

  virtual void set_type_alias( const std::string& alias_type_name,
                               const std::string& original_type_name );

  virtual void set_inst_alias( const std::string& alias_type_name,
                               const std::string& original_type_name,
                               const std::string& full_inst_path );

  //--------------------------------------------------------------------------
  // Group: Creation
  //--------------------------------------------------------------------------

  virtual uvm_object* create_object_by_type( uvm_object_wrapper* requested_type,
                                             const std::string& parent_inst_path = "",
                                             const std::string& name = "" );

  virtual uvm_component* create_component_by_type( uvm_object_wrapper* requested_type,
                                                   const std::string& parent_inst_path = "",
                                                   const std::string& name = "",
                                                   uvm_component* parent = nullptr );

  virtual uvm_object* create_object_by_name( const std::string& requested_type_name,
                                             const std::string& parent_inst_path = "",
                                             const std::string& name = "" );

  virtual uvm_component* create_component_by_name( const std::string& requested_type_name,
                                                   const std::string& parent_inst_path = "",
                                                   const std::string& name = "",
                                                   uvm_component* parent = nullptr );

  virtual bool is_type_name_registered( const std::string& type_name ) const;

  virtual bool is_type_registered( uvm_object_wrapper* obj ) const;

  //--------------------------------------------------------------------------
  // Group: Debug
  //--------------------------------------------------------------------------

  virtual void debug_create_by_type( uvm_object_wrapper* requested_type,
                                     const std::string& parent_inst_path = "",
                                     const std::string& name = "" );

  virtual void debug_create_by_name( const std::string& requested_type_name,
                                     const std::string& parent_inst_path = "",
                                     const std::string& name = "" );

  virtual uvm_object_wrapper* find_override_by_type( uvm_object_wrapper* requested_type,
                                                     const std::string& full_inst_path );

  virtual uvm_object_wrapper* find_override_by_name( const std::string& requested_type_name,
                                                     const std::string& full_inst_path );

  virtual uvm_object_wrapper* find_wrapper_by_name( const std::string& type_name );

  virtual void print( int all_types = 1 );


  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////


  bool m_delete_object( uvm_object* obj );
  void m_delete_all_objects();

  bool m_delete_component( uvm_component* comp );
  void m_delete_all_components();

 protected:
  uvm_default_factory();

  virtual ~uvm_default_factory();

  bool check_inst_override_exists( uvm_object_wrapper* original_type,
                                   uvm_object_wrapper* override_type,
                                   const std::string& full_inst_path );

  bool m_has_wildcard( const std::string& nm );

  void m_debug_create( const std::string& requested_type_name,
                       uvm_object_wrapper* requested_type,
                       const std::string& parent_inst_path,
                       const std::string& name );

  void m_debug_display( const std::string& requested_type_name,
                        uvm_object_wrapper* result,
                        const std::string& full_inst_path );

 private:

  static void cleanup();

  uvm_object_wrapper* m_resolve_type_alias( const std::string& type_name ) const;

  uvm_object_wrapper* m_resolve_type_name( const std::string& type_name,
                                           const std::string& full_inst_path ) const;

  uvm_object_wrapper* m_resolve_type_name_impl( const std::string& type_name,
                                                const std::string& full_inst_path,
                                                std::set<std::string>& visited,
                                                std::vector<std::string>* req_chain = nullptr ) const;

  void m_resolve_pending_aliases();

  void m_bind_pending_type_overrides( const std::string& match_name,
                                      uvm_object_wrapper* resolved );

  void m_migrate_inst_override_name_queue( const std::string& match_name,
                                           uvm_object_wrapper* default_target );

  void m_set_inst_override_by_name_impl( const std::string& original_type_name,
                                         const std::string& override_type_name,
                                         const std::string& full_inst_path,
                                         unsigned long long seq );

  void m_propagate_inst_alias_overrides( const std::string& alias_type_name,
                                         uvm_object_wrapper* original_type,
                                         const std::string& full_inst_path );

  uvm_factory_override* m_find_inst_override( uvm_object_wrapper* original_type,
                                              uvm_object_wrapper* override_type,
                                              const std::string& full_inst_path ) const;

  void m_register_inst_override_req_name( uvm_factory_override* ovr,
                                          const std::string& req_name );

  void m_transfer_override_metadata( uvm_factory_override* from,
                                     uvm_factory_override* to );

  void m_delete_override( uvm_factory_override* ovr );

  uvm_object_wrapper* m_find_override_by_type_impl( uvm_object_wrapper* requested_type,
                                                   const std::string& full_inst_path,
                                                   std::set<uvm_object_wrapper*>& visited_types );

  void m_update_min_override_seq( uvm_factory_override* ovr,
                                  unsigned long long seq );

  unsigned long long m_get_override_seq( const uvm_factory_override* ovr ) const;

 protected:

  // data members

  typedef std::map<uvm_object_wrapper*, bool> m_types_mapT;
  typedef m_types_mapT::iterator m_types_mapItT;

  typedef std::map<std::string, uvm_object_wrapper*> m_type_names_mapT;
  typedef m_type_names_mapT::iterator m_type_names_mapItT;

  typedef std::list<uvm_factory_override*> m_overrides_listT;
  typedef m_overrides_listT::iterator m_overrides_listItT;
  typedef std::map< std::string, bool > m_lookup_strs_mapT;

  struct uvm_factory_queue_class
  {
    m_overrides_listT queue;
  };

  m_types_mapT       m_types;
  m_type_names_mapT  m_type_names;
  m_lookup_strs_mapT m_lookup_strs;
  m_overrides_listT  m_type_overrides;
  m_overrides_listT  m_wildcard_inst_overrides;

  typedef std::map<uvm_object_wrapper*, uvm_factory_queue_class*>
            m_inst_override_queues_mapT;
  typedef m_inst_override_queues_mapT::iterator
            m_inst_override_queues_mapItT;
  typedef std::map<std::string, uvm_factory_queue_class*>
            m_inst_override_name_queues_mapT;
  typedef m_inst_override_name_queues_mapT::iterator
            m_inst_override_name_queues_mapItT;

  m_inst_override_queues_mapT  m_inst_override_queues;
  m_inst_override_name_queues_mapT m_inst_override_name_queues;

  struct m_inst_alias_entry
  {
    std::string alias_type_name;
    uvm_object_wrapper* orig_type;
    std::string orig_type_name;
    std::string full_inst_path;
  };

  typedef std::map<std::string, uvm_object_wrapper*> m_type_aliases_mapT;
  typedef std::map<std::string, std::string>         m_type_alias_names_mapT;
  typedef std::list<m_inst_alias_entry>              m_inst_aliases_listT;
  typedef std::map<std::string, m_inst_aliases_listT> m_inst_aliases_mapT;

  m_type_aliases_mapT      m_type_aliases;
  m_type_alias_names_mapT  m_type_alias_names;
  m_inst_aliases_mapT      m_inst_aliases;

 private:
  m_overrides_listT m_override_info;
  std::set<uvm_factory_override*> m_no_replace_overrides;
  unsigned long long m_next_override_seq;
  std::map<const uvm_factory_override*, unsigned long long> m_override_seq;
  std::map<const uvm_factory_override*, std::set<std::string> > m_override_req_names;
  typedef std::map<std::string, m_overrides_listT> m_inst_override_by_req_name_mapT;
  m_inst_override_by_req_name_mapT m_inst_override_by_req_name;

  static bool m_debug_pass;
};


} // namespace uvm

//////////////////////

#endif /* UVM_DEFAULT_FACTORY_H_ */
