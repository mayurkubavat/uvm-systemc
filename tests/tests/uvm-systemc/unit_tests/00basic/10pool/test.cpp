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

// Test of uvm_pool, an associative array as an object, and of
// uvm_object_string_pool, a pool of objects which are created on demand
// and found by their name, with uvm_event_pool as an example.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

void walk( const uvm_pool<int, std::string>& pool )
{
  int key = 0;

  std::cout << "forward:";
  if (pool.first(key))
    do { std::cout << " " << key; } while (pool.next(key));
  std::cout << std::endl;

  std::cout << "backward:";
  if (pool.last(key))
    do { std::cout << " " << key; } while (pool.prev(key));
  std::cout << std::endl;
}

// a type which cannot be written to a stream
struct plain
{
  int value;
};

int sc_main(int, char*[])
{
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_report_server* svr = cs->get_report_server();

  // a pool of strings by number
  uvm_pool<int, std::string> pool("pool");

  int key = 7;
  std::cout << "empty pool: num " << pool.num() << ", first " << pool.first(key)
            << ", key still " << key << std::endl;

  pool.add(30, "thirty");
  pool.add(10, "ten");
  pool.add(20, "twenty");
  pool.add(20, "twenty again");

  std::cout << "num " << pool.num() << ", exists(20) " << pool.exists(20)
            << ", exists(25) " << pool.exists(25) << std::endl;
  std::cout << "get(20) = '" << pool.get(20) << "'" << std::endl;
  std::cout << "get(25) = '" << pool.get(25) << "', num is now " << pool.num() << std::endl;

  walk(pool);

  pool.do_delete(25);
  std::cout << "a warning for a key which does not exist is expected" << std::endl;
  pool.do_delete(99);
  std::cout << "after delete: num " << pool.num() << std::endl;

  uvm_pool<int, std::string> copy("copy");
  copy.copy(pool);
  pool.do_delete(10);
  std::cout << "copy has " << copy.num() << " items, the original " << pool.num() << std::endl;
  walk(copy);

  copy.print();

  // the global pool of a type
  uvm_pool<int, std::string>::get_global_pool()->add(1, "global one");
  std::cout << "global get(1) = '" << (uvm_pool<int, std::string>::get_global(1)) << "'" << std::endl;

  // a pool of events by name
  uvm_event_pool events("events");

  uvm_event* a = events.get("a");
  uvm_event* b = events.get("b");

  std::cout << "event names: " << a->get_name() << ", " << b->get_name()
            << "; num " << events.num() << std::endl;
  std::cout << "second get returns the same event: " << (events.get("a") == a) << std::endl;

  uvm_event own("own");
  events.add("own", &own);
  std::cout << "added event found: " << (events.get("own") == &own) << std::endl;

  std::string name;
  std::cout << "names:";
  if (events.first(name))
    do { std::cout << " " << name; } while (events.next(name));
  std::cout << std::endl;

  events.do_delete("b");
  std::cout << "a warning for a name which does not exist is expected" << std::endl;
  events.do_delete("z");
  std::cout << "events left: " << events.num() << std::endl;

  uvm_event* g = uvm_event_pool::get_global("shared");
  std::cout << "global event: " << g->get_name() << ", same again: "
            << (uvm_event_pool::get_global_pool()->get("shared") == g) << std::endl;

  // a pool can hold any type, also one which cannot be printed
  uvm_pool<int, plain> plains("plains");
  plain one = { 1 };
  plains.add(4, one);
  std::cout << "plain item: " << plains.get(4).value << ", printed: "
            << (plains.sprint().find("<value>") != std::string::npos) << std::endl;

  svr->report_summarize();

  if (svr->get_severity_count(UVM_WARNING) == 2 &&
      svr->get_severity_count(UVM_ERROR) + svr->get_severity_count(UVM_FATAL) == 0)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "!! UVM TEST FAILED !!" << std::endl;

  return 0;
}
