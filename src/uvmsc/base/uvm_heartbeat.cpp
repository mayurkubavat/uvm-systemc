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

#include <systemc>
#include <sstream>

#include "uvmsc/base/uvm_heartbeat.h"
#include "uvmsc/base/uvm_component.h"
#include "uvmsc/base/uvm_coreservice_t.h"
#include "uvmsc/base/uvm_event.h"
#include "uvmsc/base/uvm_object_globals.h"
#include "uvmsc/base/uvm_root.h"
#include "uvmsc/phasing/uvm_objection.h"

namespace uvm {

//----------------------------------------------------------------------
// uvm_heartbeat_callback
//----------------------------------------------------------------------

uvm_heartbeat_callback::uvm_heartbeat_callback( const std::string& name,
                                                uvm_object* target )
: uvm_objection_callback(name), m_target(target)
{
  if (m_target == nullptr)
    m_target = uvm_coreservice_t::get()->get_root();
}

void uvm_heartbeat_callback::raised( uvm_objection* objection,
                                     uvm_object* obj,
                                     uvm_object* source_obj,
                                     const std::string& description,
                                     int count )
{
  if (obj != m_target)
    return;

  // activity of an object which was not added is counted as well
  add(source_obj);

  for (unsigned int i = 0; i < entries.size(); i++)
  {
    if (entries[i].obj == source_obj)
    {
      entries[i].cnt++;
      entries[i].last_trigger = sc_core::sc_time_stamp();
    }
  }
}

void uvm_heartbeat_callback::dropped( uvm_objection* objection,
                                      uvm_object* obj,
                                      uvm_object* source_obj,
                                      const std::string& description,
                                      int count )
{
  raised(objection, obj, source_obj, description, count);
}

void uvm_heartbeat_callback::add( uvm_object* obj )
{
  if (exists(obj))
    return;

  entry e;
  e.obj = obj;
  e.cnt = 0;
  e.last_trigger = sc_core::SC_ZERO_TIME;
  entries.push_back(e);
}

void uvm_heartbeat_callback::remove( uvm_object* obj )
{
  for (unsigned int i = 0; i < entries.size(); i++)
  {
    if (entries[i].obj == obj)
    {
      entries.erase(entries.begin() + i);
      return;
    }
  }
}

bool uvm_heartbeat_callback::exists( uvm_object* obj ) const
{
  for (unsigned int i = 0; i < entries.size(); i++)
    if (entries[i].obj == obj)
      return true;

  return false;
}

void uvm_heartbeat_callback::reset_counts()
{
  for (unsigned int i = 0; i < entries.size(); i++)
    entries[i].cnt = 0;
}

int uvm_heartbeat_callback::objects_triggered() const
{
  int triggered = 0;

  for (unsigned int i = 0; i < entries.size(); i++)
    if (entries[i].cnt != 0)
      triggered++;

  return triggered;
}

//----------------------------------------------------------------------
// Constructor
//
//! Creates a new heartbeat instance associated with \p cntxt. The
//! context is the hierarchical location that the heartbeat objections
//! will flow through and be monitored at. The \p objection associated
//! with the heartbeat is optional, if it is left null but it must be
//! set before the heartbeat monitor will activate.
//----------------------------------------------------------------------

uvm_heartbeat::uvm_heartbeat( const std::string& name,
                              uvm_component* cntxt,
                              uvm_objection* objection )
: uvm_object(name),
  m_objection(objection),
  m_cb(nullptr),
  m_cntxt(cntxt),
  m_mode(UVM_ALL_ACTIVE),
  m_event(nullptr),
  m_started(false),
  m_added(false),
  m_waiting(false)
{
  if (m_cntxt == nullptr)
    m_cntxt = uvm_coreservice_t::get()->get_root();

  m_cb = new uvm_heartbeat_callback(name + "_cb", m_cntxt);
}

//----------------------------------------------------------------------
// Destructor
//
// The objection which the heartbeat monitors shall outlive the heartbeat.
//----------------------------------------------------------------------

uvm_heartbeat::~uvm_heartbeat()
{
  try
  {
    stop();
  }
  catch (...)
  {
    // a destructor shall not let an exception of the kernel out
  }

  if (m_added)
    uvm_objection_cbs_t::do_delete(m_objection, m_cb);

  delete m_cb;
}

//----------------------------------------------------------------------
// member function: set_mode
//
//! Sets or retrieves the heartbeat mode. The current value for the
//! heartbeat mode is returned. If an argument is specified to change
//! the mode then the mode is changed to the new value.
//----------------------------------------------------------------------

uvm_heartbeat_modes uvm_heartbeat::set_mode( uvm_heartbeat_modes mode )
{
  uvm_heartbeat_modes old_mode = m_mode;

  if (mode == UVM_ANY_ACTIVE || mode == UVM_ONE_ACTIVE || mode == UVM_ALL_ACTIVE)
    m_mode = mode;

  return old_mode;
}

//----------------------------------------------------------------------
// member function: set_heartbeat
//
//! Sets up the heartbeat event and assigns a list of objects to watch.
//! The monitoring is started as soon as this member function is called.
//! Once the monitoring has been started with a specific event, providing
//! a new monitor event results in an error. To change trigger events,
//! first stop() the monitor and then start() with a new event trigger.
//!
//! If the trigger event \p e is null and there was no previously set
//! trigger event, then the monitoring is not started. Monitoring can be
//! started by explicitly calling start().
//----------------------------------------------------------------------

void uvm_heartbeat::set_heartbeat( uvm_event* e,
                                   const std::vector<uvm_component*>& comps )
{
  for (unsigned int i = 0; i < comps.size(); i++)
    m_cb->add(comps[i]);

  if (e == nullptr && m_event == nullptr)
    return;

  start(e);
}

//----------------------------------------------------------------------
// member function: add
//
//! Add a single component to the set of components to be monitored.
//! This does not cause monitoring to be started. If monitoring is
//! currently active then this component will be immediately added to
//! the list of components and will be expected to participate in the
//! currently active event window.
//----------------------------------------------------------------------

void uvm_heartbeat::add( uvm_component* comp )
{
  m_cb->add(comp);
}

//----------------------------------------------------------------------
// member function: remove
//
//! Remove a single component from the set of components being
//! monitored. Monitoring is not stopped, even if the last component
//! has been removed (an explicit stop is required).
//----------------------------------------------------------------------

void uvm_heartbeat::remove( uvm_component* comp )
{
  m_cb->remove(comp);
}

//----------------------------------------------------------------------
// member function: start
//
//! Starts the heartbeat monitor. If \p e is null then whatever event
//! was previously set is used. If no event was previously set then a
//! warning is issued. It is an error if the monitor is currently
//! running and \p e is specifying a different trigger event from the
//! current event.
//----------------------------------------------------------------------

void uvm_heartbeat::start( uvm_event* e )
{
  if (m_event == nullptr && e == nullptr)
  {
    m_cntxt->uvm_report_warning("NOEVNT", "start() was called for: " +
      get_name() + " with a null trigger and no currently set trigger",
      UVM_NONE);
    return;
  }

  // other than in UVM-SV, a null trigger keeps the current one
  if ((m_event != nullptr) && (e != nullptr) && (e != m_event) && m_started)
  {
    m_cntxt->uvm_report_error("ILHBVNT", "start() was called for: " +
      get_name() + " with trigger " + e->get_name() + " which is different " +
      "from the original trigger " + m_event->get_name(), UVM_NONE);
    return;
  }

  if (e != nullptr)
    m_event = e;

  m_enable_cb();
  m_start_hb_process();
}

//----------------------------------------------------------------------
// member function: stop
//
//! Stops the heartbeat monitor. Current state information is reset so
//! that if start() is called again the process will wait for the first
//! event trigger to start the monitoring.
//----------------------------------------------------------------------

void uvm_heartbeat::stop()
{
  m_started = false;

  // A process can only be killed while the simulation runs. Before and
  // after it there is nothing to kill: the process does not run.
  const bool running =
    (sc_core::sc_get_status() & (sc_core::SC_RUNNING | sc_core::SC_PAUSED)) != 0;

  if (running && m_process.valid() && !m_process.terminated())
  {
    m_process.kill();

    // take the process off the count of the event it was waiting for
    if (m_waiting && m_event != nullptr)
      m_event->cancel();
  }

  m_waiting = false;

  m_disable_cb();
}

//----------------------------------------------------------------------
// member function: m_start_hb_process
//
// Implementation defined
//----------------------------------------------------------------------

void uvm_heartbeat::m_start_hb_process()
{
  if (m_started)
    return;

  m_started = true;

  m_process = sc_core::sc_spawn(sc_bind(&uvm_heartbeat::m_hb_process, this),
    sc_core::sc_gen_unique_name("uvm_heartbeat_process"));
}

//----------------------------------------------------------------------
// member function: m_enable_cb
//
// Implementation defined
//----------------------------------------------------------------------

void uvm_heartbeat::m_enable_cb()
{
  m_cb->callback_mode(1);

  if (m_objection == nullptr)
    return;

  if (!m_added)
    uvm_objection_cbs_t::add(m_objection, m_cb);

  m_added = true;
}

//----------------------------------------------------------------------
// member function: m_disable_cb
//
// Implementation defined
//----------------------------------------------------------------------

void uvm_heartbeat::m_disable_cb()
{
  m_cb->callback_mode(0);
}

//----------------------------------------------------------------------
// member function: m_hb_process
//
// Implementation defined
// Checks the activity of the watched components at each trigger of the
// event, from the second trigger on.
//----------------------------------------------------------------------

void uvm_heartbeat::m_hb_process()
{
  bool triggered = false;
  sc_core::sc_time last_trigger = sc_core::SC_ZERO_TIME;
  const std::string objection_name =
    (m_objection != nullptr) ? m_objection->get_name() : std::string("");

  while (true)
  {
    m_waiting = true;
    m_event->wait_trigger();
    m_waiting = false;

    if (triggered)
    {
      std::vector<uvm_heartbeat_callback::entry>& entries = m_cb->entries;

      switch (m_mode)
      {
        case UVM_ALL_ACTIVE:
        {
          for (unsigned int i = 0; i < entries.size(); i++)
          {
            if (entries[i].cnt == 0)
            {
              std::ostringstream str;
              str << "Did not recieve an update of " << objection_name
                  << " for component " << entries[i].obj->get_full_name()
                  << " since last event trigger at time " << last_trigger
                  << " : last update time was " << entries[i].last_trigger;
              m_cntxt->uvm_report_fatal("HBFAIL", str.str(), UVM_NONE);
            }
          }
          break;
        }

        case UVM_ANY_ACTIVE:
        {
          if (!entries.empty() && m_cb->objects_triggered() == 0)
          {
            std::ostringstream str;
            str << "Did not recieve an update of " << objection_name
                << " on any component since last event trigger at time "
                << last_trigger << ". The list of registered components is:";
            for (unsigned int i = 0; i < entries.size(); i++)
              str << "\n  " << entries[i].obj->get_full_name();
            m_cntxt->uvm_report_fatal("HBFAIL", str.str(), UVM_NONE);
          }
          break;
        }

        case UVM_ONE_ACTIVE:
        {
          if (m_cb->objects_triggered() > 1)
          {
            std::ostringstream str;
            str << "Recieved update of " << objection_name
                << " from more than one component since last event trigger at time "
                << last_trigger << ". The list of triggered components is:";
            for (unsigned int i = 0; i < entries.size(); i++)
              if (entries[i].cnt != 0)
                str << "\n  " << entries[i].obj->get_full_name()
                    << " (updated: " << entries[i].last_trigger << ")";
            m_cntxt->uvm_report_fatal("HBFAIL", str.str(), UVM_NONE);
          }

          if (!entries.empty() && m_cb->objects_triggered() == 0)
          {
            std::ostringstream str;
            str << "Did not recieve an update of " << objection_name
                << " on any component since last event trigger at time "
                << last_trigger << ". The list of registered components is:";
            for (unsigned int i = 0; i < entries.size(); i++)
              str << "\n  " << entries[i].obj->get_full_name();
            m_cntxt->uvm_report_fatal("HBFAIL", str.str(), UVM_NONE);
          }
          break;
        }

        default:
          break;
      }
    }

    m_cb->reset_counts();
    last_trigger = sc_core::sc_time_stamp();
    triggered = true;
  }
}

} // namespace uvm
