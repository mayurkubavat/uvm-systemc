//----------------------------------------------------------------------
//   Copyright 2023 Intel Corporation
//   Copyright 2023-2024 NVIDIA Corporation
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

#include <functional>
#include <sstream>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/base/uvm_coreservice_t.h"
#include "uvmsc/base/uvm_root.h"
#include "uvmsc/dpi/uvm_hdl.h"
#include "uvmsc/dpi/uvm_hdl_polling.h"
#include "uvmsc/cb/uvm_callback_iter.h"
#include "uvmsc/macros/uvm_message_defines.h"

namespace uvm {

//----------------------------------------------------------------------
// class uvm_hdl_polling_cbs
//----------------------------------------------------------------------

// The pair is registered here, so that callbacks can be added for all
// polling objects before any polling object exists.
uvm_hdl_polling_cbs::uvm_hdl_polling_cbs( const std::string& name )
: uvm_callback(name)
{
  uvm_callbacks<uvm_hdl_polling, uvm_hdl_polling_cbs>::m_register_pair(
    "uvm_hdl_polling", "uvm_hdl_polling_cbs");
}

//----------------------------------------------------------------------
// class uvm_polling_backdoor
//----------------------------------------------------------------------

uvm_polling_backdoor::uvm_polling_backdoor( const std::string& name )
: uvm_object(name), key(-1)
{}

bool uvm_polling_backdoor::create_backdoor_probe( int, const std::string&, bool )
{
  uvm_report_fatal("UVM/HDL/POLLING/Backdoor",
    "uvm_polling_backdoor::create_backdoor_probe() method has not been overloaded", UVM_NONE);
  return false;
}

void uvm_polling_backdoor::poll_bkdr_wait_for_hdl_change( uvm_poll_status_e&, uvm_poll_data_t& )
{
  uvm_report_fatal("UVM/HDL/POLLING/Backdoor",
    "uvm_polling_backdoor::poll_bkdr_wait_for_hdl_change() method has not been overloaded", UVM_NONE);
}

void uvm_polling_backdoor::hdl_read( uvm_poll_status_e& status, uvm_poll_data_t& )
{
  status = UVM_HDL_POLL_NOT_OK;
  uvm_report_fatal("UVM/HDL/POLLING/Backdoor",
    "uvm_polling_backdoor::hdl_read_func() method has not been overloaded", UVM_NONE);
}

int uvm_polling_backdoor::get_signal_size()
{
  uvm_report_fatal("UVM/HDL/POLLING/Backdoor",
    "uvm_polling_backdoor::get_signal_size() method has not been overloaded", UVM_NONE);
  return 0;
}

void uvm_polling_backdoor::set_hdl_path( const std::string& path )
{
  hdl_backdoor_path = path;
}

const std::string uvm_polling_backdoor::get_type_name() const
{
  return "uvm_polling_backdoor";
}

//----------------------------------------------------------------------
// class uvm_hdl_polling
//----------------------------------------------------------------------

int uvm_hdl_polling::uvm_hdl_polling_instance_count = 0;

std::map<std::string, uvm_hdl_polling*>& uvm_hdl_polling::m_probes_by_name()
{
  // never deleted, so that polling objects with static storage may use it
  // when they are destroyed
  static std::map<std::string, uvm_hdl_polling*>* probes_by_name =
    new std::map<std::string, uvm_hdl_polling*>();
  return *probes_by_name;
}

uvm_hdl_polling* uvm_hdl_polling::m_find_probe( const std::string& fullname )
{
  std::map<std::string, uvm_hdl_polling*>::iterator it = m_probes_by_name().find(fullname);
  return (it == m_probes_by_name().end()) ? nullptr : it->second;
}

uvm_hdl_polling::uvm_hdl_polling( const std::string& name )
: uvm_object(name),
  key(-1),
  m_hdl_poll_cb_registered(false),
  m_bkdr(nullptr),
  m_signal(nullptr),
  m_alive(std::make_shared<bool>(true))
{
  m_stat_info.num_path_cbs = 0;
  m_stat_info.num_obj_val_changes = 0;
  m_stat_info.signal_size = 0;
}

//----------------------------------------------------------------------
// Destructor
//
// A polling object which is deleted stops polling, and its path can be
// given to another one. The backdoor stays with its owner. Callbacks which
// were added to this object stay in the callback pool, which keeps them
// by its address: remove them with uvm_hdl_polling_cb::do_delete() first.
//----------------------------------------------------------------------

uvm_hdl_polling::~uvm_hdl_polling()
{
  *m_alive = false;

  try
  {
    m_stop_monitor();
  }
  catch (...)
  {
    // a destructor shall not let an exception of the kernel out
  }

  std::map<std::string, uvm_hdl_polling*>::iterator it = m_probes_by_name().find(signal_name);
  if (it != m_probes_by_name().end() && it->second == this)
    m_probes_by_name().erase(it);
}

void uvm_hdl_polling::m_stop_monitor()
{
  // a process can only be killed while the simulation runs
  const bool running =
    (sc_core::sc_get_status() & (sc_core::SC_RUNNING | sc_core::SC_PAUSED)) != 0;

  // the process which runs the callbacks may delete the object; it ends by
  // itself then, as it finds the object gone
  if (running && m_monitor.valid() && !m_monitor.terminated() &&
      m_monitor != sc_core::sc_get_current_process_handle())
    m_monitor.kill();
}

uvm_polling_backdoor* uvm_hdl_polling::get_backdoor() const
{
  return m_bkdr;
}

void uvm_hdl_polling::set_backdoor( uvm_polling_backdoor* backdoor )
{
  m_bkdr = backdoor;
}

void uvm_hdl_polling::set_hdl_poll_cb_registered( bool value )
{
  m_hdl_poll_cb_registered = value;
}

bool uvm_hdl_polling::get_hdl_poll_cb_registered() const
{
  return m_hdl_poll_cb_registered;
}

//----------------------------------------------------------------------
// member function: create_probe
//
//! Connects the polling object \p p to the signal \p fullname, and
//! starts watching it. Other than in UVM-SV, a second probe on the same
//! signal is refused, so that the first one keeps working.
//----------------------------------------------------------------------

bool uvm_hdl_polling::create_probe( uvm_hdl_polling* p, const std::string& fullname, bool enable )
{
  if (m_find_probe(fullname) != nullptr)
  {
    uvm_report_error("UVM/HDL/POLLING",
      "Duplicate signal probe on signal \"" + fullname + "\"", UVM_NONE);
    return false;
  }

  const int new_key = uvm_hdl_polling_instance_count;
  sc_core::sc_interface* signal = nullptr;

  if (m_bkdr == nullptr)
  {
    std::string why;
    signal = m_find_signal(fullname, why);

    if (signal == nullptr)
    {
      uvm_report_fatal("UVM/HDL/POLLING", "create_probe for path " + fullname +
        " could not create probe: " + why, UVM_NONE);
      return false;
    }
  }
  else if (!m_bkdr->create_backdoor_probe(new_key, fullname, enable))
  {
    uvm_report_fatal("UVM/HDL/POLLING", "create_probe for path " + fullname +
      " could not create probe in User defined Backdoor", UVM_NONE);
    return false;
  }

  uvm_hdl_polling_instance_count++;

  m_probes_by_name()[fullname] = p;
  p->signal_name = fullname;
  p->key = new_key;
  p->set_hdl_poll_cb_registered(enable);

  p->m_signal = signal;
  p->m_spawn_monitor();

  return true;
}

//----------------------------------------------------------------------
// member function: m_find_signal
//
// Implementation defined
// The signal of a path, which the built-in mechanism can watch and read:
// an sc_signal<uvm_poll_data_t, SC_MANY_WRITERS>, or an object which is
// both a uvm_sc_if<uvm_poll_data_t> and a SystemC interface with a
// default event. The path is resolved as uvm_hdl_read() does.
//----------------------------------------------------------------------

sc_core::sc_interface* uvm_hdl_polling::m_find_signal( const std::string& fullname,
                                                       std::string& why )
{
  sc_core::sc_object* obj = sc_core::sc_find_object(fullname.c_str());

  if (obj == nullptr)
  {
    std::string objname1, objname2;
    int idx_start = -1, idx_stop = -1, vidx_start = -1, vidx_stop = -1;

    uvm_extract_path_index(fullname, objname1, idx_start, idx_stop);
    uvm_extract_path_index(objname1, objname2, vidx_start, vidx_stop);
    obj = sc_core::sc_find_object(objname2.c_str());
  }

  if (obj == nullptr)
  {
    why = "no such object";
    return nullptr;
  }

  typedef sc_core::sc_signal<uvm_poll_data_t, sc_core::SC_MANY_WRITERS> poll_signal;

  if (poll_signal* sig = dynamic_cast<poll_signal*>(obj))
    return sig;

  sc_core::sc_interface* iface = dynamic_cast<sc_core::sc_interface*>(obj);

  if (iface != nullptr && dynamic_cast<uvm_sc_if<uvm_poll_data_t>*>(obj) != nullptr)
    return iface;

  why = "the object is not an sc_signal<uvm_poll_data_t, SC_MANY_WRITERS>, nor a "
        "uvm_sc_if<uvm_poll_data_t> with a value change event; use a uvm_polling_backdoor";
  return nullptr;
}

//----------------------------------------------------------------------
// member function: m_spawn_monitor
//
// Implementation defined
// Starts the process which watches the signal. With SystemC 3 (IEEE
// 1666-2023) it is created in the scope of uvm_root, so that it does not
// die with the process which created the probe, a sequence which is
// killed for instance. With older kernels it is a child of that process:
// m_ensure_monitor() starts it again when the polling object is used.
//----------------------------------------------------------------------

void uvm_hdl_polling::m_spawn_monitor()
{
  uvm_root* root = uvm_coreservice_t::get()->get_root();

  if (m_bkdr == nullptr)
    m_monitor = root->m_spawn(std::bind(&uvm_hdl_polling::m_monitor_signal, this, m_alive, m_signal),
      "uvm_hdl_polling");
  else
    m_monitor = root->m_spawn(std::bind(&uvm_hdl_polling::m_monitor_backdoor, this, m_alive),
      "uvm_hdl_polling");
}

void uvm_hdl_polling::m_ensure_monitor()
{
  if (!signal_name.empty() && m_monitor.valid() && m_monitor.terminated() &&
      sc_core::sc_is_running())
  {
    uvm_report_info("UVM/HDL/POLLING", "the process which polls " + signal_name +
      " was killed; it is started again", UVM_HIGH);
    m_spawn_monitor();
  }
}

//----------------------------------------------------------------------
// member function: register_hdl_path
//
//! Gives this polling object the signal \p hdl_path_name
//----------------------------------------------------------------------

bool uvm_hdl_polling::register_hdl_path( const std::string& hdl_path_name )
{
  if (!create_probe(this, hdl_path_name))
  {
    uvm_report_fatal("UVM/HDL/POLL", "Could not register a path in object " +
      get_full_name() + " for signal " + hdl_path_name + ". Check your path or your " +
      "backdoor methods if you are using the backdoor", UVM_NONE);
    return false;
  }

  m_stat_info.num_path_cbs++;
  m_stat_info.full_signal_path = hdl_path_name;
  m_stat_info.object_name = get_full_name();
  m_stat_info.signal_size = (m_bkdr == nullptr) ? UVM_REG_DATA_WIDTH : m_bkdr->get_signal_size();

  set_hdl_poll_cb_registered(true);

  uvm_report_info("UVM/HDL/POLL", "Successfully registered a callback in object " +
    get_full_name() + " for signal " + hdl_path_name + ".", UVM_DEBUG);

  return true;
}

//----------------------------------------------------------------------
// member function: hdl_polling_execute_callbacks
//
//! Runs the callbacks of this object with the current value of the signal
//----------------------------------------------------------------------

void uvm_hdl_polling::hdl_polling_execute_callbacks()
{
  uvm_poll_data_t val = 0;
  int size;

  if (m_bkdr == nullptr)
  {
    m_read(val);
    size = UVM_REG_DATA_WIDTH;
  }
  else
  {
    uvm_poll_status_e status;
    size = m_bkdr->get_signal_size();
    m_bkdr->hdl_read(status, val);
  }

  uvm_callback_iter<uvm_hdl_polling, uvm_hdl_polling_cbs> iter(this);

  for (uvm_hdl_polling_cbs* cb = iter.first(); cb != nullptr; cb = iter.next())
    cb->do_on_path_change(m_stat_info.full_signal_path, val, size);
}

//----------------------------------------------------------------------
// member function: wait_for_hdl_change
//
//! Waits for the next change of the signal, and returns its value
//----------------------------------------------------------------------

void uvm_hdl_polling::wait_for_hdl_change( uvm_poll_status_e& status, uvm_poll_data_t& val )
{
  if (!get_hdl_poll_cb_registered())
    return;

  m_ensure_monitor();

  if (m_bkdr == nullptr)
  {
    sc_core::wait(changed);
    m_stat_info.num_obj_val_changes++;
    status = m_read(val) ? UVM_HDL_POLL_OK : UVM_HDL_POLL_NOT_OK;
  }
  else
    m_bkdr->poll_bkdr_wait_for_hdl_change(status, val);
}

//----------------------------------------------------------------------
// member function: polling_wait_for_value
//
//! Waits until the signal has the value \p exp_val; returns at once if
//! it has it already
//----------------------------------------------------------------------

void uvm_hdl_polling::polling_wait_for_value( uvm_poll_status_e& status,
                                              const uvm_poll_data_t& exp_val )
{
  uvm_poll_data_t val = 0;

  if (!get_hdl_poll_cb_registered())
  {
    status = UVM_HDL_POLL_NOT_OK;
    return;
  }

  status = UVM_HDL_POLL_OK;
  m_ensure_monitor();

  if (m_bkdr == nullptr)
    m_read(val);
  else
    m_bkdr->hdl_read(status, val);

  while (val != exp_val)
  {
    if (m_bkdr == nullptr)
      wait_for_hdl_change(status, val);
    else
      m_bkdr->poll_bkdr_wait_for_hdl_change(status, val);
  }
}

std::string uvm_hdl_polling::convert2string() const
{
  std::ostringstream str;
  str << "Statistics info:" << std::endl
      << "  Total Path polling instances = " << get_number_callbacks() << std::endl
      << "   Total path vpi callback registered = " << get_value_change_count() << std::endl
      << "  Total path value changes = " << 0 << std::endl << "  ";
  return str.str();
}

int uvm_hdl_polling::get_number_callbacks() const
{
  return m_stat_info.num_path_cbs;
}

int uvm_hdl_polling::get_value_change_count() const
{
  return m_stat_info.num_obj_val_changes;
}

void uvm_hdl_polling::m_polling_releaseWaiters()
{
  changed.notify();
  hdl_polling_execute_callbacks();
}

void uvm_hdl_polling::uvm_polling_waitForChange()
{
  sc_core::wait(changed);
}

void uvm_hdl_polling::uvm_hdl_polling_monitor_backdoor()
{
  uvm_poll_status_e status;
  uvm_poll_data_t val;

  // the object is deleted while waiting only if the process was killed
  while (true)
  {
    m_bkdr->poll_bkdr_wait_for_hdl_change(status, val);
    m_polling_releaseWaiters();
  }
}

//----------------------------------------------------------------------
// member function: m_monitor_signal
//
// Implementation defined
// The built-in mechanism: waits for the default event of the signal,
// which is its value changed event for an sc_signal
//----------------------------------------------------------------------

void uvm_hdl_polling::m_monitor_signal( uvm_hdl_polling* p, std::shared_ptr<bool> alive,
                                        sc_core::sc_interface* signal )
{
  while (*alive)
  {
    sc_core::wait(signal->default_event());

    if (*alive)
      p->m_polling_releaseWaiters();
  }
}

// The loop of uvm_hdl_polling_monitor_backdoor(), checking after each
// change that the object still exists: a callback may delete it.
void uvm_hdl_polling::m_monitor_backdoor( uvm_hdl_polling* p, std::shared_ptr<bool> alive )
{
  uvm_poll_status_e status;
  uvm_poll_data_t val;

  while (*alive)
  {
    p->m_bkdr->poll_bkdr_wait_for_hdl_change(status, val);

    if (*alive)
      p->m_polling_releaseWaiters();
  }
}

bool uvm_hdl_polling::m_read( uvm_poll_data_t& val )
{
  return uvm_hdl_read(m_stat_info.full_signal_path, val);
}

//----------------------------------------------------------------------
// Global functions
//----------------------------------------------------------------------

uvm_hdl_polling* uvm_get_poll( const std::string& pathname )
{
  uvm_hdl_polling* poll_inst = uvm_hdl_polling::m_find_probe(pathname);

  if (poll_inst != nullptr)
  {
    poll_inst->m_ensure_monitor();
    return poll_inst;
  }

  std::ostringstream name;
  name << "uvm_hdl_polling_get_poll_" << (uvm_hdl_polling::uvm_hdl_polling_instance_count + 1);

  poll_inst = uvm_hdl_polling::type_id::create(name.str());
  poll_inst->register_hdl_path(pathname);
  return poll_inst;
}

bool uvm_set_poll( uvm_hdl_polling* poll_inst, const std::string& pathname )
{
  if (poll_inst == nullptr)
  {
    uvm_report_error("UVM/HDL/POLLING", "Polling instance is null. Cannot set poll instance", UVM_NONE);
    return false;
  }

  return poll_inst->register_hdl_path(pathname);
}

} // namespace uvm
