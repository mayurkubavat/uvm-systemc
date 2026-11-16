//----------------------------------------------------------------------
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2011 Cadence Design Systems, Inc.
//   Copyright 2010-2011 Synopsys, Inc.
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

#ifndef UVM_HEARTBEAT_H_
#define UVM_HEARTBEAT_H_

#include <systemc>
#include <memory>
#include <string>
#include <vector>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/phasing/uvm_objection_callback.h"

namespace uvm {

// forward class declarations
class uvm_component;
class uvm_event;
class uvm_objection;

//----------------------------------------------------------------------
// Enum: uvm_heartbeat_modes
//
//! Defines how the heartbeat is checked at each trigger of its event.
//!
//! UVM_ALL_ACTIVE - All components must report activity
//! UVM_ONE_ACTIVE - Exactly one component must report activity
//! UVM_ANY_ACTIVE - At least one component must report activity
//! UVM_NO_HB_MODE - No mode, used to query the current mode
//----------------------------------------------------------------------

enum uvm_heartbeat_modes
{
  UVM_ALL_ACTIVE,
  UVM_ONE_ACTIVE,
  UVM_ANY_ACTIVE,
  UVM_NO_HB_MODE
};

//----------------------------------------------------------------------
// CLASS: uvm_heartbeat_callback
//
// Implementation defined
// Counts the objection activity of the watched components, as seen at
// the target of the heartbeat.
//----------------------------------------------------------------------

class uvm_heartbeat_callback : public uvm_objection_callback
{
 public:
  struct entry
  {
    uvm_object*      obj;
    int              cnt;
    sc_core::sc_time last_trigger;
  };

  uvm_heartbeat_callback( const std::string& name, uvm_object* target );

  virtual void raised( uvm_objection* objection,
                       uvm_object* obj,
                       uvm_object* source_obj,
                       const std::string& description,
                       int count );

  virtual void dropped( uvm_objection* objection,
                        uvm_object* obj,
                        uvm_object* source_obj,
                        const std::string& description,
                        int count );

  void add( uvm_object* obj );
  void remove( uvm_object* obj );
  bool exists( uvm_object* obj ) const;

  void reset_counts();
  int objects_triggered() const;

  // the watched objects, in the order in which they were added
  std::vector<entry> entries;

 private:
  uvm_object* m_target;

}; // class uvm_heartbeat_callback

//----------------------------------------------------------------------
// CLASS: uvm_heartbeat
//
//! Heartbeats provide a way for environments to easily ensure that
//! their descendants are alive. A uvm_heartbeat is associated with a
//! specific objection object. A component that is being tracked by the
//! heartbeat object must raise (or drop) the synchronizing objection
//! during the heartbeat window.
//!
//! The uvm_heartbeat object has a list of participating objects. The
//! heartbeat can be configured so that all components (UVM_ALL_ACTIVE),
//! exactly one (UVM_ONE_ACTIVE), or any component (UVM_ANY_ACTIVE)
//! must trigger the objection in order to satisfy the heartbeat
//! condition.
//----------------------------------------------------------------------

class uvm_heartbeat : public uvm_object
{
 public:
  //--------------------------------------------------------------------
  // UVM Standard LRM API below
  //--------------------------------------------------------------------

  uvm_heartbeat( const std::string& name,
                 uvm_component* cntxt,
                 uvm_objection* objection = nullptr );

  virtual ~uvm_heartbeat();

  uvm_heartbeat_modes set_mode( uvm_heartbeat_modes mode = UVM_NO_HB_MODE );

  void set_heartbeat( uvm_event* e, const std::vector<uvm_component*>& comps );

  void add( uvm_component* comp );

  void remove( uvm_component* comp );

  void start( uvm_event* e = nullptr );

  void stop();

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

 private:
  // heartbeats refer to components and to an event, they are not copied
  uvm_heartbeat( const uvm_heartbeat& );
  uvm_heartbeat& operator=( const uvm_heartbeat& );

  void m_start_hb_process();
  void m_enable_cb();
  void m_disable_cb();
  void m_hb_process( std::shared_ptr<bool> run );
  static void m_run_hb_process( uvm_heartbeat* hb, std::shared_ptr<bool> run );

  uvm_objection*          m_objection;
  uvm_heartbeat_callback* m_cb;
  uvm_component*          m_cntxt;
  uvm_heartbeat_modes     m_mode;
  uvm_event*              m_event;
  bool                    m_started;
  bool                    m_added;
  bool                    m_waiting; // the process waits for the event

  sc_core::sc_process_handle m_process;

  // true while the process may run; set to false by stop(). A process
  // which is killed before it ran still starts, so it checks this first,
  // without touching the heartbeat, which may be deleted by then.
  std::shared_ptr<bool> m_run;

}; // class uvm_heartbeat

} // namespace uvm

#endif // UVM_HEARTBEAT_H_
