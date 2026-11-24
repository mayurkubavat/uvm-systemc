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

// Test of uvm_tlm_fifo: a producer and a consumer connected through
// the exports of a bounded FIFO, the member functions size, used,
// is_empty, is_full and flush, a FIFO without limit, and the analysis
// ports put_ap and get_ap.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class producer : public uvm_component
{
 public:
  uvm_blocking_put_port<int> put_port;

  UVM_COMPONENT_UTILS(producer);

  producer( uvm_component_name name )
  : uvm_component(name), put_port("put_port") {}

  void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    for (int i = 1; i <= 6; i++)
    {
      put_port.put(i * 10);
      std::cout << sc_core::sc_time_stamp() << " producer: put "
                << i * 10 << std::endl;
    }

    phase.drop_objection(this);
  }
};

class consumer : public uvm_component
{
 public:
  uvm_get_peek_port<int> get_port;

  UVM_COMPONENT_UTILS(consumer);

  consumer( uvm_component_name name )
  : uvm_component(name), get_port("get_port") {}

  void run_phase( uvm_phase& phase )
  {
    int p, v;

    phase.raise_objection(this);

    for (int i = 0; i < 6; i++)
    {
      sc_core::wait(10, sc_core::SC_NS);
      get_port.peek(p);
      get_port.get(v);
      std::cout << sc_core::sc_time_stamp() << " consumer: peek " << p
                << ", get " << v << std::endl;
    }

    phase.drop_objection(this);
  }
};

class listener : public uvm_subscriber<int>
{
 public:
  std::string what;
  int count;

  listener( uvm_component_name name, const std::string& what_ )
  : uvm_subscriber<int>(name), what(what_), count(0) {}

  virtual void write( const int& t )
  {
    count++;
  }
};

class test : public uvm_test
{
 public:
  producer prod;
  consumer cons;
  uvm_tlm_fifo<int> fifo;
  uvm_tlm_fifo<int> local_fifo;
  uvm_tlm_fifo<int> unbounded_fifo;
  uvm_tlm_fifo<int> unnamed_fifo;
  listener put_listener;
  listener get_listener;
  listener local_put_listener;
  listener local_get_listener;

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name )
  : uvm_test(name),
    prod("prod"),
    cons("cons"),
    fifo("fifo", 2),
    local_fifo("local_fifo", 3),
    unbounded_fifo("unbounded_fifo", 0),
    unnamed_fifo(4),
    put_listener("put_listener", "put"),
    get_listener("get_listener", "get"),
    local_put_listener("local_put_listener", "put"),
    local_get_listener("local_get_listener", "get")
  {}

  virtual void connect_phase( uvm_phase& phase )
  {
    prod.put_port.connect(fifo.blocking_put_export);
    cons.get_port.connect(fifo.get_peek_export);
    fifo.put_ap.connect(put_listener);
    fifo.get_ap.connect(get_listener);
    local_fifo.put_ap.connect(local_put_listener);
    local_fifo.get_ap.connect(local_get_listener);
  }

  void show( const std::string& what, const uvm_tlm_fifo<int>& f )
  {
    std::cout << what << ": ";
    if (f.is_unbounded())
      std::cout << "no limit";
    else
      std::cout << "size " << f.size();
    std::cout << ", used " << f.used()
              << ", is_empty " << f.is_empty() << ", is_full " << f.is_full()
              << std::endl;
  }

  void run_phase( uvm_phase& phase )
  {
    int v = 0;

    phase.raise_objection(this);

    show("local_fifo, new", local_fifo);
    std::cout << "can_get " << local_fifo.can_get()
              << ", try_get " << local_fifo.try_get(v) << std::endl;

    for (int i = 1; i <= 4; i++)
      std::cout << "try_put(" << i << ") returns "
                << local_fifo.try_put(i) << std::endl;

    // what has been put can be read from the next delta cycle
    sc_core::wait(sc_core::SC_ZERO_TIME);
    show("local_fifo, after 4 x try_put", local_fifo);
    std::cout << "can_put " << local_fifo.can_put() << std::endl;

    local_fifo.try_peek(v);
    std::cout << "try_peek gives " << v << std::endl;
    local_fifo.get(v);
    std::cout << "get gives " << v << std::endl;
    v = 0;
    local_fifo.peek(v);
    std::cout << "peek gives " << v << std::endl;
    std::cout << "try_get returns " << local_fifo.try_get(v)
              << " and gives " << v << std::endl;
    show("local_fifo, after get and try_get", local_fifo);

    // only what was put and retrieved is written to the analysis ports
    std::cout << "local_fifo: put_ap wrote " << local_put_listener.count
              << " transactions, get_ap wrote " << local_get_listener.count
              << std::endl;

    local_fifo.flush();
    show("local_fifo, after flush", local_fifo);

    // the places become available in the next delta cycle
    sc_core::wait(sc_core::SC_ZERO_TIME);
    show("local_fifo, one delta cycle later", local_fifo);

    std::cout << "local_fifo: get_ap wrote " << local_get_listener.count
              << " transactions after flush" << std::endl;

    show("unnamed_fifo", unnamed_fifo);
    show("unbounded_fifo, new", unbounded_fifo);
    for (int i = 0; i < 100; i++)
      if (!unbounded_fifo.try_put(i))
        UVM_ERROR("TEST", "try_put failed on a FIFO without limit");
    sc_core::wait(sc_core::SC_ZERO_TIME);
    show("unbounded_fifo, after 100 x try_put", unbounded_fifo);

    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    show("fifo, at the end", fifo);
    std::cout << "put_ap wrote " << put_listener.count
              << " transactions, get_ap wrote " << get_listener.count
              << std::endl;
    std::cout << "type name " << fifo.get_type_name() << std::endl;

    if (put_listener.count == 6 && get_listener.count == 6 &&
        uvm_report_server::get_server()->get_severity_count(UVM_ERROR) == 0)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "** UVM TEST FAILED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  test t("t");
  run_test();
  return 0;
}
