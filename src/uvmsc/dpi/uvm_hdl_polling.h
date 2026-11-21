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

#ifndef UVM_HDL_POLLING_H_
#define UVM_HDL_POLLING_H_

#include <map>
#include <memory>
#include <string>

#include <systemc>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/cb/uvm_callback.h"
#include "uvmsc/cb/uvm_callbacks.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_callback_defines.h"
#include "uvmsc/macros/uvm_object_defines.h"
#include "uvmsc/reg/uvm_reg_model.h"
#include "uvmsc/reg/uvm_sc_if.h"

//----------------------------------------------------------------------
// TITLE: UVM HDL polling
//
// Lets a testbench wait for a value change of a signal in the design,
// found by its hierarchical name, and run callbacks when it changes.
//
// In UVM-SV the built-in mechanism uses the VPI. In UVM-SystemC a
// signal is an object of the SystemC hierarchy: the built-in mechanism
// waits for the default event of the object, which for an sc_signal is
// its value changed event, and reads the value as uvm_hdl_read() does,
// so the object shall be an sc_signal<uvm_poll_data_t, SC_MANY_WRITERS>.
// Other objects are polled through a user-defined uvm_polling_backdoor.
//----------------------------------------------------------------------

namespace uvm {

class uvm_hdl_polling;

typedef uvm_reg_data_t uvm_poll_data_t;

enum uvm_poll_status_e
{
  UVM_HDL_POLL_NOT_OK,
  UVM_HDL_POLL_OK
};

struct uvm_hdl_poll_data_info
{
  int num_path_cbs;
  int num_obj_val_changes;
  std::string full_signal_path;
  int signal_size;
  std::string object_name;
};

//----------------------------------------------------------------------
// CLASS: uvm_hdl_polling_cbs
//
//! Callbacks which run when a polled signal changes
//----------------------------------------------------------------------

class uvm_hdl_polling_cbs : public uvm_callback
{
 public:
  explicit uvm_hdl_polling_cbs( const std::string& name = "" );

  //! To be implemented by extended classes to perform an action upon a
  //! value change of a register or a field
  virtual void do_on_hdl_change( const std::string& full_path, int signal_size,
                                 const uvm_poll_data_t& val ) {}

  //! To be implemented by extended classes to perform an action upon a
  //! value change of a path. \p hdl_path is the path whose value has
  //! changed, \p val its value after the change, and \p size the size in
  //! bits of the value.
  virtual void do_on_path_change( const std::string& hdl_path,
                                  const uvm_poll_data_t& val, int size ) {}
};

typedef uvm_callbacks<uvm_hdl_polling, uvm_hdl_polling_cbs> uvm_hdl_polling_cb;

//----------------------------------------------------------------------
// CLASS: uvm_polling_backdoor
//
//! A user-defined way to poll a signal, for signals which the built-in
//! mechanism cannot reach. An extension implements all its methods.
//----------------------------------------------------------------------

class uvm_polling_backdoor : public uvm_object
{
 public:
  std::string hdl_backdoor_path;
  int key;

  explicit uvm_polling_backdoor( const std::string& name = "" );

  virtual bool create_backdoor_probe( int key, const std::string& fullname,
                                      bool enable = true );

  virtual void poll_bkdr_wait_for_hdl_change( uvm_poll_status_e& status,
                                              uvm_poll_data_t& val );

  virtual void hdl_read( uvm_poll_status_e& status, uvm_poll_data_t& val );

  virtual int get_signal_size();

  void set_hdl_path( const std::string& path );

  virtual const std::string get_type_name() const;
};

//----------------------------------------------------------------------
// CLASS: uvm_hdl_polling
//
//! Polls one signal of the design. A polling object is created for a
//! path with uvm_get_poll(), or created by the user and given a path
//! with register_hdl_path().
//----------------------------------------------------------------------

class uvm_hdl_polling : public uvm_object
{
 public:
  UVM_OBJECT_UTILS(uvm_hdl_polling);
  UVM_REGISTER_CB(uvm_hdl_polling, uvm_hdl_polling_cbs);

  std::string signal_name;
  int key;

  explicit uvm_hdl_polling( const std::string& name = "uvm_hdl_polling" );
  virtual ~uvm_hdl_polling();

  uvm_polling_backdoor* get_backdoor() const;
  void set_backdoor( uvm_polling_backdoor* backdoor );

  void set_hdl_poll_cb_registered( bool value );
  bool get_hdl_poll_cb_registered() const;

  bool create_probe( uvm_hdl_polling* p, const std::string& fullname, bool enable = true );

  bool register_hdl_path( const std::string& hdl_path_name );

  void hdl_polling_execute_callbacks();

  virtual void wait_for_hdl_change( uvm_poll_status_e& status, uvm_poll_data_t& val );

  virtual void polling_wait_for_value( uvm_poll_status_e& status,
                                       const uvm_poll_data_t& exp_val );

  virtual std::string convert2string() const;

  int get_number_callbacks() const;
  int get_value_change_count() const;

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  virtual void m_polling_releaseWaiters();
  virtual void uvm_polling_waitForChange();
  virtual void uvm_hdl_polling_monitor_backdoor();

  //! The polling object of a path, or nullptr
  static uvm_hdl_polling* m_find_probe( const std::string& fullname );

  //! The number of polling objects which got a path
  static int uvm_hdl_polling_instance_count;

 protected:
  sc_core::sc_event changed;

 private:
  static void m_monitor_signal( uvm_hdl_polling* p, std::shared_ptr<bool> alive,
                                sc_core::sc_interface* signal );
  static void m_monitor_backdoor( uvm_hdl_polling* p, std::shared_ptr<bool> alive );
  bool m_read( uvm_poll_data_t& val );
  void m_stop_monitor();
  void m_spawn_monitor();
  void m_ensure_monitor();
  static sc_core::sc_interface* m_find_signal( const std::string& fullname, std::string& why );

  friend uvm_hdl_polling* uvm_get_poll( const std::string& pathname );

  static std::map<std::string, uvm_hdl_polling*>& m_probes_by_name();

  bool m_hdl_poll_cb_registered;
  uvm_hdl_poll_data_info m_stat_info;
  uvm_polling_backdoor* m_bkdr;
  sc_core::sc_process_handle m_monitor;
  sc_core::sc_interface* m_signal;

  // false once the object is deleted; the process which watches the
  // signal checks it, as it cannot always be killed (before it ran)
  std::shared_ptr<bool> m_alive;
};

//----------------------------------------------------------------------
// Global functions
//----------------------------------------------------------------------

//! Returns the polling object of \p pathname, and creates one if there
//! is none. A polling object created here lives until the program ends.
uvm_hdl_polling* uvm_get_poll( const std::string& pathname );

//! Gives the polling object \p poll_inst the path \p pathname
bool uvm_set_poll( uvm_hdl_polling* poll_inst, const std::string& pathname );

} // namespace uvm

#endif // UVM_HDL_POLLING_H_
