//----------------------------------------------------------------------
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

// A testbench polls signals of the design: it waits for their changes
// and for values, and callbacks run on each change. A signal which the
// built-in mechanism cannot reach is polled through a user backdoor.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

// the design: a signal which counts up every 10 ns
SC_MODULE(dut)
{
  sc_core::sc_signal<uvm_poll_data_t, sc_core::SC_MANY_WRITERS> sig;
  sc_core::sc_signal<uvm_poll_data_t, sc_core::SC_MANY_WRITERS> sig2;
  int hidden; // not an sc_signal: only a backdoor can poll it
  sc_core::sc_event hidden_changed;

  SC_CTOR(dut) : sig("sig"), sig2("sig2"), hidden(0)
  {
    SC_THREAD(count);
  }

  void count()
  {
    for (int i = 1; i <= 12; i++)
    {
      sc_core::wait(5, sc_core::SC_NS);
      sig.write(i);
      sig2.write(i);
      sc_core::wait(5, sc_core::SC_NS);
      hidden = 100 + i;
      hidden_changed.notify();
    }
  }
};

dut* top = nullptr;

class printer_cb : public uvm_hdl_polling_cbs
{
 public:
  int seen;

  printer_cb( const std::string& name ) : uvm_hdl_polling_cbs(name), seen(0) {}

  virtual void do_on_path_change( const std::string& hdl_path,
                                  const uvm_poll_data_t& val, int size )
  {
    seen++;
    std::cout << sc_core::sc_time_stamp() << ": " << get_name() << ": " << hdl_path
              << " is " << val.to_uint64() << " (" << size << " bits)" << std::endl;
  }
};

class hidden_backdoor : public uvm_polling_backdoor
{
 public:
  hidden_backdoor() : uvm_polling_backdoor("hidden_backdoor") {}

  virtual bool create_backdoor_probe( int k, const std::string& fullname, bool )
  {
    key = k;
    set_hdl_path(fullname);
    return true;
  }

  virtual void poll_bkdr_wait_for_hdl_change( uvm_poll_status_e& status, uvm_poll_data_t& val )
  {
    sc_core::wait(top->hidden_changed);
    status = UVM_HDL_POLL_OK;
    val = top->hidden;
  }

  virtual void hdl_read( uvm_poll_status_e& status, uvm_poll_data_t& val )
  {
    status = UVM_HDL_POLL_OK;
    val = top->hidden;
  }

  virtual int get_signal_size() { return 32; }
};

// counts the changes
class counter_cb : public uvm_hdl_polling_cbs
{
 public:
  int seen;

  counter_cb() : uvm_hdl_polling_cbs("counter"), seen(0) {}

  virtual void do_on_path_change( const std::string&, const uvm_poll_data_t&, int )
  {
    seen++;
  }
};

// deletes its polling object at the first change
class deleter_cb : public uvm_hdl_polling_cbs
{
 public:
  uvm_hdl_polling* poll;

  deleter_cb( const std::string& name ) : uvm_hdl_polling_cbs(name), poll(nullptr) {}

  virtual void do_on_path_change( const std::string& hdl_path,
                                  const uvm_poll_data_t& val, int size )
  {
    if (poll != nullptr)
    {
      std::cout << sc_core::sc_time_stamp() << ": a callback deletes its polling object" << std::endl;
      uvm_hdl_polling* p = poll;
      poll = nullptr;
      delete p;
    }
  }
};

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  printer_cb all;      // for every polling object
  printer_cb only_sig; // for the polling object of the signal
  hidden_backdoor bkdr;
  uvm_hdl_polling* hidden_poll;

  test( uvm_component_name name )
  : uvm_test(name), all("all"), only_sig("only_sig"), hidden_poll(nullptr) {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // a callback for all polling objects, added before any exists
    uvm_hdl_polling_cb::add(nullptr, &all);
  }

  counter_cb counter;

  void poll_and_wait()
  {
    uvm_hdl_polling_cb::add(uvm_get_poll("top.sig2"), &counter);
    sc_core::wait(sc_core::SC_ZERO_TIME);
    sc_core::wait(1000, sc_core::SC_NS);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    uvm_hdl_polling* p = uvm_get_poll("top.sig");
    std::cout << "the same object for the same path: " << (uvm_get_poll("top.sig") == p) << std::endl;
    uvm_hdl_polling_cb::add(p, &only_sig);

    hidden_poll = new uvm_hdl_polling("hidden_poll");
    hidden_poll->set_backdoor(&bkdr);
    std::cout << "backdoor probe: " << uvm_set_poll(hidden_poll, "top.hidden") << std::endl;

    uvm_poll_status_e status;
    uvm_poll_data_t val;

    p->wait_for_hdl_change(status, val);
    std::cout << sc_core::sc_time_stamp() << ": changed to " << val.to_uint64()
              << ", ok " << (status == UVM_HDL_POLL_OK) << std::endl;

    p->polling_wait_for_value(status, 4);
    std::cout << sc_core::sc_time_stamp() << ": the signal is 4" << std::endl;

    p->polling_wait_for_value(status, 4);
    std::cout << sc_core::sc_time_stamp() << ": still 4, no wait" << std::endl;

    hidden_poll->polling_wait_for_value(status, 105);
    std::cout << sc_core::sc_time_stamp() << ": the hidden value is 105 through the backdoor" << std::endl;

    sc_core::wait(20, sc_core::SC_NS);

    std::cout << "changes waited for: " << p->get_value_change_count()
              << ", paths: " << p->get_number_callbacks() << std::endl;
    std::cout << "callbacks: all " << all.seen << ", only_sig " << only_sig.seen << std::endl;

    // a deleted polling object stops polling and frees its path
    delete hidden_poll;
    hidden_poll = new uvm_hdl_polling("hidden_poll_2");
    hidden_poll->set_backdoor(&bkdr);
    std::cout << "the path can be polled again: " << uvm_set_poll(hidden_poll, "top.hidden") << std::endl;
    delete hidden_poll;

    // a polling object created by a process which is killed, as a sequence
    sc_core::sc_process_handle owner = sc_core::sc_spawn(sc_bind(&test::poll_and_wait, this));
    sc_core::wait(1, sc_core::SC_NS);
    owner.kill(sc_core::SC_INCLUDE_DESCENDANTS);
    const int seen_at_kill = counter.seen;
    sc_core::wait(20, sc_core::SC_NS);
    std::cout << "callbacks go on after the process which added them was killed: "
              << (counter.seen == seen_at_kill + 2) << std::endl;
    uvm_hdl_polling* p2 = uvm_get_poll("top.sig2");
    p2->wait_for_hdl_change(status, val);
    std::cout << sc_core::sc_time_stamp() << ": the polling object of a killed process still polls: "
              << val.to_uint64() << std::endl;

    deleter_cb deleter("deleter");
    deleter.poll = p2;
    uvm_hdl_polling_cb::do_delete(p2, &counter);
    uvm_hdl_polling_cb::add(p2, &deleter);
    sc_core::wait(15, sc_core::SC_NS);
    std::cout << "a polling object deleted by its callback frees its path: "
              << (uvm_hdl_polling::m_find_probe("top.sig2") == nullptr) << std::endl;

    // the same through a backdoor
    uvm_hdl_polling* hidden_3 = new uvm_hdl_polling("hidden_3");
    hidden_3->set_backdoor(&bkdr);
    uvm_set_poll(hidden_3, "top.hidden");
    deleter_cb bkdr_deleter("bkdr_deleter");
    bkdr_deleter.poll = hidden_3;
    uvm_hdl_polling_cb::add(hidden_3, &bkdr_deleter);
    sc_core::wait(15, sc_core::SC_NS);
    std::cout << "a backdoor polling object deleted by its callback frees its path: "
              << (uvm_hdl_polling::m_find_probe("top.hidden") == nullptr) << std::endl;

    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  top = new dut("top");
  run_test("test");
  delete top;
  return 0;
}
